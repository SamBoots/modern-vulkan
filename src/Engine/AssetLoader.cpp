#include "AssetLoader.hpp"

#include "Storage/Hashmap.h"
#include "Storage/Array.h"
#include "BBIntrin.h"
#include "BBImage.hpp"
#include "Math.inl"
#include "Program.h"

#include "shared_common.hlsl.h"
#include "Renderer.hpp"

#include "MemoryInterfaces.hpp"

#include "BBThreadScheduler.hpp"

#include "Storage/Queue.hpp"

#include "MaterialSystem.hpp"

BB_WARNINGS_OFF
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define STB_IMAGE_RESIZE2_IMPLEMENTATION
#include "stb_image_resize2.h"
BB_WARNINGS_ON

using namespace BB;

constexpr size_t MAX_PATH_SIZE = 512;
constexpr size_t MAX_ASSET_NAME_SIZE = 64;

constexpr size_t GPU_TASK_QUEUE_SIZE = 64;

constexpr size_t MAX_STRING_SIZE_STORAGE = 2024;

constexpr const char TEXTURE_DIRECTORY[] = "../../resources/textures/";
constexpr const char ICON_DIRECTORY[] = "../../resources/icons/";

using PathString = StackString<MAX_PATH_SIZE>;
using AssetString = StackString<MAX_ASSET_NAME_SIZE>;

static inline IMAGE_FORMAT ImageFormatFromChannels(const int a_channels)
{
	switch (a_channels)
	{
	case 4:
		return IMAGE_FORMAT::RGBA8_SRGB;
	case 3:
		return IMAGE_FORMAT::RGB8_SRGB;
	case 1:
		return IMAGE_FORMAT::A8_UNORM;
	default:
		BB_ASSERT(false, "unsupported channel count");
		return IMAGE_FORMAT::RGB8_SRGB;	
	}
}

static bool IsPathImage(const StringView a_view)
{
	const size_t extension_pos = a_view.find_last_of('.');

	if (a_view.compare(".png", extension_pos) || a_view.compare(".jpg", extension_pos) || a_view.compare(".bmp", extension_pos))
		return true;
	return false;
}

static char* CreateGLTFImagePath(MemoryArena& a_temp_arena, const char* a_image_path)
{
	const size_t image_path_size = strlen(a_image_path);
	const size_t texture_directory_size = sizeof(TEXTURE_DIRECTORY);
	char* new_path = ArenaAllocArr(a_temp_arena, char, texture_directory_size + image_path_size);

	Memory::Copy(new_path, TEXTURE_DIRECTORY, sizeof(TEXTURE_DIRECTORY));
	Memory::Copy(&new_path[sizeof(TEXTURE_DIRECTORY) - 1], a_image_path, image_path_size);
	new_path[sizeof(TEXTURE_DIRECTORY) + image_path_size - 1] = '\0';

	return new_path;
}

//crappy hash, don't care for now.
static uint64_t StringHash(const char* a_string, const size_t a_size)
{
	uint64_t hash = 5381;
	uint32_t c;

	for (size_t i = 0; i < a_size; i++)
	{
		c = *reinterpret_cast<const uint8_t*>(a_string++);
		hash = ((hash << 5) + hash) + c;
	}

	return hash;
}

static uint64_t TurboCrappyImageHash(const void* a_pixels, const size_t a_byte_size)
{
	size_t remaining = a_byte_size;
	VecUint4 b128_hash = LoadUint4Zero();

	const size_t diff = Pointer::AlignForwardAdjustment(a_pixels, sizeof(VecUint4));
	a_pixels = Pointer::Add(a_pixels, diff);

	while (remaining >= sizeof(VecUint4))
	{
		b128_hash = AddUint4(b128_hash, LoadUint4(reinterpret_cast<const uint32_t*>(a_pixels)));
		a_pixels = Pointer::Add(a_pixels, sizeof(VecUint4));
		remaining -= sizeof(VecUint4);
	}

	const uint64_t* b128_to_b64 = reinterpret_cast<const uint64_t*>(&b128_hash);

	const size_t size_addition = a_byte_size ^ 8;

	return b128_to_b64[0] + b128_to_b64[1] + size_addition;
}

enum class ASSET_TYPE : uint8_t
{
	MODEL,
	IMAGE
};

//this hash works by setting a 64 bit hash, then overwriting the last byte to show which asset type it is.
union AssetHash
{
	uint64_t full_hash;
	struct
	{
		ASSET_TYPE type;	// 1
		uint8_t hash[7];	// 8
	};
};

static AssetHash CreateAssetHash(const uint64_t a_hash, const ASSET_TYPE a_type)
{
	AssetHash hash;
	hash.full_hash = a_hash;
	hash.type = a_type;
	return hash;
}

constexpr uint2 ICON_EXTENT = uint2(64, 64);
constexpr float2 ICON_EXTENT_F = float2(64.f, 64.f);

struct IconSlot
{
	uint32_t slot_index;
	uint32_t next_slot;
};

struct AssetSlot
{
	AssetHash hash;
	const char* path;
	const char* name;
	IconSlot icon;
	union
	{
		Model* model;
		Image* image;
	};
};

typedef void (*PFN_GPUTaskCallback)(const void* a_params);

struct GPUTask
{
	GPUFenceValue transfer_value;
	PFN_GPUTaskCallback callback;
	void* params;
};

struct AssetManager
{
	// asset storage
	BBRWLock asset_lock;
	MemoryArena asset_arena;
	StaticOL_HashMap<uint64_t, AssetSlot> asset_table;
	StaticArray<AssetSlot*> linear_asset_table;

	BBRWLock gpu_task_lock;
	FreelistInterface gpu_task_arena;
	SPSCQueue<GPUTask> gpu_tasks_queue;

	struct IconGigaTexture
	{
		IconSlot empty_slot;
		BBRWLock icon_lock;
		RTexture texture;
		IconSlot* slots;
		uint32_t max_slots;	// a slot is a texture space that is equal to ICON_EXTENT
		uint32_t next_slot;
	};

	IconGigaTexture icons_storage;

	// string storage
	BBRWLock string_lock;
	MemoryArena string_arena;
	// change char* to a string class 
	StaticOL_HashMap<uint64_t, char*> string_table;
};
static AssetManager* s_asset_manager;

template<typename T>
static void AddGPUTask(const PFN_GPUTaskCallback a_callback, const T& a_params, const GPUFenceValue a_fence_value)
{
	OSAcquireSRWLockWrite(&s_asset_manager->gpu_task_lock);
	GPUTask task;
	task.transfer_value = a_fence_value;
	task.callback = a_callback;
	task.params = s_asset_manager->gpu_task_arena.Alloc(sizeof(T), alignof(T));
	*reinterpret_cast<T*>(task.params) = a_params;
	s_asset_manager->gpu_tasks_queue.EnQueue(task);
	OSReleaseSRWLockWrite(&s_asset_manager->gpu_task_lock);
}

static void ExecuteGPUTasks()
{
	const GPUFenceValue fence_value = GetTransferFenceValue();

	OSAcquireSRWLockWrite(&s_asset_manager->gpu_task_lock);
	const GPUTask* task = s_asset_manager->gpu_tasks_queue.Peek();
	while (task && task->transfer_value.handle <= fence_value.handle)
	{
		task->callback(task->params);
		// free the param memory
		s_asset_manager->gpu_task_arena.Free(task->params);
		s_asset_manager->gpu_tasks_queue.DeQueue();
		task = s_asset_manager->gpu_tasks_queue.Peek();
	}
	OSReleaseSRWLockWrite(&s_asset_manager->gpu_task_lock);
}

static inline AssetString GetAssetIconName(const StringView& a_asset_name)
{
	StackString<MAX_ASSET_NAME_SIZE> icon_name(a_asset_name.c_str(), a_asset_name.size());
	icon_name.append(" icon");
	return icon_name;
}

static inline void GetAssetNameFromPath(const char* a_path, AssetString& a_asset_name)
{
	StringView path_view = a_path;
	const size_t name_start = path_view.find_last_of_directory_slash();
	BB_ASSERT(name_start != size_t(-1), "cannot find a directory slash");
	const size_t name_end = path_view.find_last_of('.');
	BB_ASSERT(name_end != size_t(-1), "file has no file extension name");
	a_asset_name.append(&a_path[name_start + 1], name_end - name_start - 1);
}

static inline PathString GetIconPathFromAssetName(const StringView& a_asset_name)
{
	PathString icon_path(ICON_DIRECTORY);
	icon_path.append(GetAssetIconName(a_asset_name));
	icon_path.append(".png");
	return icon_path;
}

static inline IconSlot GetEmptyIconSlot()
{
	return s_asset_manager->icons_storage.empty_slot;
}

static inline IconSlot GetIconSlotSpace()
{
	OSAcquireSRWLockWrite(&s_asset_manager->icons_storage.icon_lock);
	BB_ASSERT(s_asset_manager->icons_storage.next_slot != UINT32_MAX, "icon storage full");
	const IconSlot icon_slot = s_asset_manager->icons_storage.slots[s_asset_manager->icons_storage.next_slot];
	s_asset_manager->icons_storage.next_slot = icon_slot.next_slot;
	OSReleaseSRWLockWrite(&s_asset_manager->icons_storage.icon_lock);
	return icon_slot;
}

static inline void* ResizeImage(MemoryArena& a_arena, const void* a_src_pixels, const int a_src_width, const int a_src_height, const int a_dst_width, const int a_dst_height)
{
	unsigned char* icon_pixels = ArenaAllocArr(a_arena, unsigned char, ICON_EXTENT.x * ICON_EXTENT.y * 4);

	BB_ASSERT(stbir_resize_uint8_srgb(
		reinterpret_cast<const unsigned char*>(a_src_pixels), a_src_width, a_src_height, 0,
		icon_pixels, a_dst_width, a_dst_height, 0, STBIR_RGBA), "failed to resize image using stibr");

	return icon_pixels;
}

struct IconWriteToDisk_params
{
	GPUBuffer readback;
	uint2 image_extent;
	PathString write_path;
};

static inline void IconWriteToDisk_impl(const void* a_params)
{
	const IconWriteToDisk_params* params = reinterpret_cast<const IconWriteToDisk_params*>(a_params);

	const void* mapped = MapGPUBuffer(params->readback);
	BB_WARNING(Asset::WriteImage(params->write_path.c_str(), params->image_extent.x, params->image_extent.y, 4, mapped), "failed to write icon image to disk", WarningType::MEDIUM);
	UnmapGPUBuffer(params->readback);
	FreeGPUBuffer(params->readback);
}

static inline bool IconWriteToDisk(const IconSlot a_slot, const PathString& a_write_path)
{
	GPUBufferCreateInfo readback_buff;
	readback_buff.name = "icon readback";
	readback_buff.size = static_cast<size_t>(ICON_EXTENT.x * ICON_EXTENT.y) * 4; // 4 color channels
	readback_buff.host_writable = true;
	readback_buff.type = BUFFER_TYPE::READBACK;
	const GPUBuffer readback = CreateGPUBuffer(readback_buff);

	const int2 read_offset(static_cast<int>(a_slot.slot_index * ICON_EXTENT.x), 0);

	const GPUFenceValue fence_value = ReadTexture(s_asset_manager->icons_storage.texture, ICON_EXTENT, read_offset, readback, readback_buff.size);

	IconWriteToDisk_params params;
	params.readback = readback;
	params.image_extent = ICON_EXTENT;
	params.write_path = a_write_path;

	AddGPUTask(IconWriteToDisk_impl, params, fence_value);
	return true;
}

static inline IconSlot LoadIconFromPath(MemoryArena& a_temp_arena, const StringView a_icon_path, const bool a_set_icons_shader_visible)
{
	int width = 0, height = 0, channels = 0;
	stbi_uc* pixels = stbi_load(a_icon_path.c_str(), &width, &height, &channels, 4);

	const void* write_pixels = pixels;

	if (!pixels || width != static_cast<int>(ICON_EXTENT.x) || height != static_cast<int>(ICON_EXTENT.y))
	{
		write_pixels = ResizeImage(a_temp_arena, pixels, width, height, ICON_EXTENT.x, ICON_EXTENT.y);
	}

	const IconSlot slot = GetIconSlotSpace();

	WriteTextureInfo write_icon_info;
	write_icon_info.extent = ICON_EXTENT;
	write_icon_info.offset = int2(static_cast<int>(slot.slot_index * ICON_EXTENT.x), 0);
	write_icon_info.pixels = write_pixels;
	write_icon_info.set_shader_visible = a_set_icons_shader_visible;
	write_icon_info.layer_count = 1;
	write_icon_info.base_array_layer = 0;
	WriteTexture(s_asset_manager->icons_storage.texture, write_icon_info);

	return slot;
}

static inline IconSlot LoadIconFromPixels(const void* a_pixels, const bool a_set_icons_shader_visible)
{
	const IconSlot slot = GetIconSlotSpace();

	WriteTextureInfo write_icon_info;
	write_icon_info.extent = ICON_EXTENT;
	write_icon_info.offset = int2(static_cast<int>(slot.slot_index * ICON_EXTENT.x), 0);
	write_icon_info.pixels = a_pixels;
	write_icon_info.set_shader_visible = a_set_icons_shader_visible;
	write_icon_info.layer_count = 1;
	write_icon_info.base_array_layer = 0;
	WriteTexture(s_asset_manager->icons_storage.texture, write_icon_info);

	return slot;
}

static inline StringView AllocateStringSpace(const char* a_str, const size_t a_string_size, const uint64_t a_hash)
{
	OSAcquireSRWLockWrite(&s_asset_manager->string_lock);
	char* const string = ArenaAllocArr(s_asset_manager->string_arena, char, a_string_size);
	memcpy(string, a_str, a_string_size);
	string[a_string_size - 1] = '\0';

	s_asset_manager->string_table.emplace(a_hash, string);
	OSReleaseSRWLockWrite(&s_asset_manager->string_lock);
	return StringView(string, a_string_size - 1);
}

static inline void AddElementToAssetTable(AssetSlot& a_asset_slot)
{
	OSAcquireSRWLockWrite(&s_asset_manager->asset_lock);
	s_asset_manager->asset_table.insert(a_asset_slot.hash.full_hash, a_asset_slot);
	AssetSlot* slot = s_asset_manager->asset_table.find(a_asset_slot.hash.full_hash);
	s_asset_manager->linear_asset_table.push_back(slot);
	OSReleaseSRWLockWrite(&s_asset_manager->asset_lock);
}

using namespace BB;

void Asset::InitializeAssetManager(const AssetManagerInitInfo& a_init_info)
{
	BB_ASSERT(!s_asset_manager, "Asset Manager already initialized");

	{	//initialize the memory arena and place it into the struct itself.
		MemoryArena asset_mem_arena = MemoryArenaCreate();

		s_asset_manager = ArenaAllocType(asset_mem_arena, AssetManager);
	
		s_asset_manager->asset_arena = asset_mem_arena;
	}

	s_asset_manager->asset_lock = OSCreateRWLock();
	s_asset_manager->string_lock = OSCreateRWLock();
	s_asset_manager->asset_table.Init(s_asset_manager->asset_arena, a_init_info.asset_count);
	s_asset_manager->linear_asset_table.Init(s_asset_manager->asset_arena, a_init_info.asset_count);
	s_asset_manager->string_table.Init(s_asset_manager->asset_arena, a_init_info.string_entry_count);

	s_asset_manager->gpu_task_lock = OSCreateRWLock();
	s_asset_manager->gpu_task_arena.Initialize(s_asset_manager->asset_arena, mbSize * 64);
	s_asset_manager->gpu_tasks_queue.Init(s_asset_manager->asset_arena, GPU_TASK_QUEUE_SIZE);

	s_asset_manager->icons_storage.icon_lock = OSCreateRWLock();
	s_asset_manager->icons_storage.max_slots = a_init_info.asset_count;
	s_asset_manager->icons_storage.slots = ArenaAllocArr(s_asset_manager->asset_arena, IconSlot, a_init_info.asset_count);
	s_asset_manager->icons_storage.next_slot = 1;
	CreateTextureInfo icons_texture_info;
	icons_texture_info.name = "icon mega texture";
	icons_texture_info.width = ICON_EXTENT.x * s_asset_manager->icons_storage.max_slots;
	icons_texture_info.height = ICON_EXTENT.y;
	icons_texture_info.usage = IMAGE_USAGE::TEXTURE;
	icons_texture_info.format = IMAGE_FORMAT::RGBA8_SRGB;
	icons_texture_info.array_layers = 1;
	s_asset_manager->icons_storage.texture = CreateTexture(icons_texture_info);

	//slot 0 is for debug
	s_asset_manager->icons_storage.empty_slot = { 0, 0 };

	for (uint32_t i = s_asset_manager->icons_storage.next_slot; i < s_asset_manager->icons_storage.max_slots - 1; i++)
	{
		s_asset_manager->icons_storage.slots[i].slot_index = i;
		s_asset_manager->icons_storage.slots[i].next_slot = i + 1;
	}
	s_asset_manager->icons_storage.slots[s_asset_manager->icons_storage.max_slots - 1].slot_index = s_asset_manager->icons_storage.max_slots - 1;
	s_asset_manager->icons_storage.slots[s_asset_manager->icons_storage.max_slots - 1].next_slot = UINT32_MAX;

	s_asset_manager->string_arena = MemoryArenaCreate();

	// create some directories for files we are going to write
	if (!OSDirectoryExist(ICON_DIRECTORY))
		BB_ASSERT(OSCreateDirectory(ICON_DIRECTORY), "failed to create ICON directory");
}

void Asset::Update()
{
	ExecuteGPUTasks();
}

StringView Asset::FindOrCreateString(const char* a_string)
{
	return FindOrCreateString(StringView(a_string, strnlen_s(a_string, MAX_STRING_SIZE_STORAGE)));
}

StringView Asset::FindOrCreateString(const char* a_string, const size_t a_string_size)
{
	return FindOrCreateString(StringView(a_string, a_string_size));
}

StringView Asset::FindOrCreateString(const StringView& a_view)
{
	const uint64_t string_hash = StringHash(a_view.c_str(), a_view.size());
	char** string_ptr = s_asset_manager->string_table.find(string_hash);
	if (string_ptr != nullptr)
		return *string_ptr;

	const uint32_t string_size = static_cast<uint32_t>(a_view.size() + 1);

	return AllocateStringSpace(a_view.c_str(), string_size, string_hash);
}

Slice<Asset::LoadedAssetInfo> Asset::LoadAssets(MemoryArena& a_temp_arena, const Slice<AsyncAsset> a_asyn_assets)
{
	LoadedAssetInfo* loaded_assets = ArenaAllocArr(a_temp_arena, LoadedAssetInfo, a_asyn_assets.size());
	for (size_t i = 0; i < a_asyn_assets.size(); i++)
	{
		const AsyncAsset& task = a_asyn_assets[i];
		loaded_assets[i].type = task.asset_type;
		switch (task.asset_type)
		{
		case ASYNC_ASSET_TYPE::MODEL:
		{
			switch (task.load_type)
			{
			case ASYNC_LOAD_TYPE::DISK:
				loaded_assets[i].name = LoadglTFModel(a_temp_arena, task.mesh_disk);
				break;
			case ASYNC_LOAD_TYPE::MEMORY:
				loaded_assets[i].name = LoadMeshFromMemory(a_temp_arena, task.mesh_memory);
				break;
			}
			break;
		}
		case ASYNC_ASSET_TYPE::TEXTURE:
		{
			switch (task.load_type)
			{
			case ASYNC_LOAD_TYPE::DISK:
				loaded_assets[i].name = LoadImageDisk(a_temp_arena, task.texture_disk.path);
				break;
			case ASYNC_LOAD_TYPE::MEMORY:
				loaded_assets[i].name = LoadImageMemory(a_temp_arena, *task.texture_memory.image, task.texture_memory.name);
				break;
			}
		}
		break;
		}
	}

	return Slice(loaded_assets, a_asyn_assets.size());
}

struct LoadAsyncFunc_Params
{
	MemoryArena memory_arena;
	Asset::AsyncAsset* assets;
	size_t asset_count;
};

static void LoadAsync_func(MemoryArena&, void* a_param)
{
	using namespace Asset;
	LoadAsyncFunc_Params* params = reinterpret_cast<LoadAsyncFunc_Params*>(a_param);

	LoadAssets(params->memory_arena, Slice(params->assets, params->asset_count));

	MemoryArenaFree(params->memory_arena);
}

const StringView Asset::LoadImageDisk(MemoryArena& a_temp_arena, const char* a_path)
{
	AssetString asset_name;
	GetAssetNameFromPath(a_path, asset_name);
	StringView image_name = FindOrCreateString(asset_name.GetView());

	int width = 0, height = 0, channels = 0;
	stbi_uc* pixels = stbi_load(a_path, &width, &height, &channels, 4);

	CreateTextureInfo create_image_info;
	create_image_info.name = image_name.c_str();
	create_image_info.width = static_cast<uint32_t>(width);
	create_image_info.height = static_cast<uint32_t>(height);
	create_image_info.format = ImageFormatFromChannels(4);
	create_image_info.usage = IMAGE_USAGE::TEXTURE;
	create_image_info.array_layers = 1;

	WriteTextureInfo write_info{};
	write_info.pixels = pixels;
	write_info.extent = { create_image_info.width, create_image_info.height };
	write_info.set_shader_visible = true;
	write_info.layer_count = 1;
	write_info.base_array_layer = 0;
	const RTexture gpu_image = CreateTexture(create_image_info);
	WriteTexture(gpu_image, write_info);
	
	OSAcquireSRWLockWrite(&s_asset_manager->asset_lock);
	Image* image = ArenaAllocType(s_asset_manager->asset_arena, Image);
	OSReleaseSRWLockWrite(&s_asset_manager->asset_lock);
	image->width = create_image_info.width;
	image->height = create_image_info.height;
	image->gpu_image = gpu_image;

	const uint64_t path_hash = StringHash(a_path, strnlen_s(a_path, MAX_PATH_SIZE));

	AssetSlot asset;
	asset.hash = CreateAssetHash(path_hash, ASSET_TYPE::IMAGE);
	asset.path = FindOrCreateString(a_path).c_str(); //memory loads have nullptr as path.
	asset.image = image;
	asset.name = image_name.c_str();

	const PathString icon_path = GetIconPathFromAssetName(asset.name);
	if (OSFileExist(icon_path.c_str()))
	{
		asset.icon = LoadIconFromPath(a_temp_arena, icon_path.GetView(), true);
	}
	else
	{
		const void* icon_write = pixels;
		if (create_image_info.width != ICON_EXTENT.x || create_image_info.height != ICON_EXTENT.y)
		{
			icon_write = ResizeImage(a_temp_arena, pixels, static_cast<int>(create_image_info.width), static_cast<int>(create_image_info.height), static_cast<int>(ICON_EXTENT.x), static_cast<int>(ICON_EXTENT.y));
		}
		asset.icon = LoadIconFromPixels(icon_write, true);
		IconWriteToDisk(asset.icon, icon_path);
	}

	AddElementToAssetTable(asset);
	asset.image->asset_handle = AssetHandle(asset.hash.full_hash);

	STBI_FREE(pixels);

	return asset.path;
}

bool Asset::WriteImage(const char* a_file_name, const uint32_t a_width, const uint32_t a_height, const uint32_t a_channels, const void* a_pixels)
{
	if (stbi_write_png(a_file_name, static_cast<int>(a_width), static_cast<int>(a_height), static_cast<int>(a_channels), a_pixels, static_cast<int>(a_width * a_channels)))
		return true;

	if (const char* stbi_fail_msg = stbi_failure_reason())
		BB_WARNING(false, stbi_fail_msg, WarningType::HIGH);
	return false;
}

unsigned char* Asset::LoadImageCPU(const char* a_file_name, int& a_width, int& a_height, int& a_bytes_per_pixel)
{
	stbi_uc* pixels = stbi_load(a_file_name, &a_width, &a_height, &a_bytes_per_pixel, 4);
	a_bytes_per_pixel = 4;
	return pixels;
}

void Asset::FreeImageCPU(void* a_pixels)
{
	STBI_FREE(a_pixels);
}

const StringView Asset::LoadImageMemory(MemoryArena& a_temp_arena, const BB::BBImage& a_image, const char* a_name)
{
	const StringView image_name = FindOrCreateString(a_name);

	CreateTextureInfo create_image_info;
	create_image_info.name = image_name.c_str();
	create_image_info.width = a_image.GetWidth();
	create_image_info.height = a_image.GetHeight();
	create_image_info.usage = IMAGE_USAGE::TEXTURE;
	create_image_info.array_layers = 1;
	switch (a_image.GetBytesPerPixel())
	{
	case 8:
		create_image_info.format = IMAGE_FORMAT::RGBA16_UNORM;
		break;
	case 4:
		create_image_info.format = IMAGE_FORMAT::RGBA8_SRGB;
		break;
	case 3:
		create_image_info.format = IMAGE_FORMAT::RGB8_SRGB;
		break;
	case 1:
		create_image_info.format = IMAGE_FORMAT::A8_UNORM;
		break;
	default:
		BB_ASSERT(false, "Current unsupported image bitcount.");
		create_image_info.format = IMAGE_FORMAT::RGBA8_SRGB;
		break;
	}

	WriteTextureInfo write_info{};
	write_info.pixels = a_image.GetPixels();
	write_info.extent = { create_image_info.width, create_image_info.height };
	write_info.set_shader_visible = true;
	write_info.layer_count = 1;
	write_info.base_array_layer = 0;
	const RTexture gpu_image = CreateTexture(create_image_info);
	WriteTexture(gpu_image, write_info);

	OSAcquireSRWLockWrite(&s_asset_manager->asset_lock);
	Image* image = ArenaAllocType(s_asset_manager->asset_arena, Image);
	OSReleaseSRWLockWrite(&s_asset_manager->asset_lock);
	image->width = create_image_info.width;
	image->height = create_image_info.height;
	image->gpu_image = gpu_image;

	const uint64_t hash = TurboCrappyImageHash(a_image.GetPixels(), static_cast<size_t>(image->width) + image->height + a_image.GetBytesPerPixel());
	// BB_ASSERT(hash != 0, "Image hashing failed");

	AssetSlot asset;
	asset.hash = CreateAssetHash(hash, ASSET_TYPE::IMAGE);
	asset.path = nullptr; // memory loads have nullptr has path.
	asset.image = image;
	asset.name = image_name.c_str();

	const PathString icon_path = GetIconPathFromAssetName(asset.name);
	if (OSFileExist(icon_path.c_str()))
	{
		asset.icon = LoadIconFromPath(a_temp_arena, icon_path.GetView(), true);
	}
	else
	{
		const void* icon_write = a_image.GetPixels();
		if (create_image_info.width != ICON_EXTENT.x || create_image_info.height != ICON_EXTENT.y)
		{
			icon_write = ResizeImage(a_temp_arena, a_image.GetPixels(), static_cast<int>(create_image_info.width), static_cast<int>(create_image_info.height), static_cast<int>(ICON_EXTENT.x), static_cast<int>(ICON_EXTENT.y));
		}
		asset.icon = LoadIconFromPixels(icon_write, true);
		IconWriteToDisk(asset.icon, icon_path);
	}


	AddElementToAssetTable(asset);
	image->asset_handle = AssetHandle(asset.hash.full_hash);

	return image_name;
}

static inline size_t CgltfGetMeshIndex(const cgltf_data& a_cgltf_data, const cgltf_mesh* a_mesh)
{
	return (reinterpret_cast<size_t>(a_mesh) - reinterpret_cast<size_t>(a_cgltf_data.meshes)) / sizeof(cgltf_mesh);
}

static void LoadglTFNode(const cgltf_data& a_cgltf_data, Model& a_model, uint32_t& a_node_index)
{
	const cgltf_node& cgltf_node = a_cgltf_data.nodes[a_node_index];
	Model::Node& node = a_model.linear_nodes[a_node_index++];
	if (cgltf_node.has_matrix)
	{
		float4x4 matrix;
		// maybe do conversion on row or not
		memcpy(&matrix, cgltf_node.matrix, sizeof(float4x4));

		Float4x4DecomposeTransform(matrix, node.translation, node.rotation, node.scale);
	}
	else
	{
		if (cgltf_node.has_translation)
			memcpy(&node.translation, cgltf_node.translation, sizeof(cgltf_node.translation));
		else
			node.translation = {};
		if (cgltf_node.has_rotation)
			memcpy(&node.rotation, cgltf_node.rotation, sizeof(cgltf_node.rotation));
		else
			node.rotation = {};
		if (cgltf_node.has_scale)
			memcpy(&node.scale, cgltf_node.scale, sizeof(cgltf_node.scale));
		else
			node.scale = float3(1.f, 1.f, 1.f);
	}

	if (cgltf_node.name)
		node.name = Asset::FindOrCreateString(cgltf_node.name).c_str();
	else
		node.name = "unnamed";
	
	if (cgltf_node.mesh != nullptr)
		node.mesh = &a_model.meshes[CgltfGetMeshIndex(a_cgltf_data, cgltf_node.mesh)];
	else
		node.mesh = nullptr;

	node.child_count = static_cast<uint32_t>(cgltf_node.children_count);
	if (node.child_count != 0)
	{
		node.childeren = &a_model.linear_nodes[a_node_index]; //childeren are loaded linearly, i'm quite sure.
		for (size_t i = 0; i < node.child_count; i++)
		{
			LoadglTFNode(a_cgltf_data, a_model, a_node_index);
		}
	}
}

static inline void* GetAccessorDataPtr(const cgltf_accessor* a_accessor)
{
	const size_t accessor_offset = a_accessor->buffer_view->offset + a_accessor->offset;
	return Pointer::Add(a_accessor->buffer_view->buffer->data, accessor_offset);
}

static void LoadglTFMesh(MemoryArena& a_temp_arena, const cgltf_mesh& a_cgltf_mesh, Model::Mesh& a_mesh)
{
	const cgltf_mesh& mesh = a_cgltf_mesh;

	uint32_t index_count = 0;
	uint32_t vertex_count = 0;
	for (size_t prim_index = 0; prim_index < mesh.primitives_count; prim_index++)
	{
		const cgltf_primitive& prim = mesh.primitives[prim_index];
		index_count += static_cast<uint32_t>(prim.indices->count);

		for (size_t attrib_index = 0; attrib_index < prim.attributes_count; attrib_index++)
		{
			const cgltf_attribute& attri = prim.attributes[attrib_index];
			if (attri.type == cgltf_attribute_type_position)
			{
				BB_ASSERT(attri.data->type == cgltf_type_vec3, "GLTF position type is not a vec3!");
				vertex_count += static_cast<uint32_t>(attri.data->count);
			}
		}
	}

	uint32_t* indices = ArenaAllocArr(a_temp_arena, uint32_t, index_count);
	Vertex* vertices = ArenaAllocArr(a_temp_arena, Vertex, vertex_count);
	uint32_t index_offset = 0;
	uint32_t vertex_pos_offset = 0;
	uint32_t vertex_normal_offset = 0;
	uint32_t vertex_uv_offset = 0;

	for (size_t prim_index = 0; prim_index < mesh.primitives_count; prim_index++)
	{
		const cgltf_primitive& prim = mesh.primitives[prim_index];
		Model::Primitive& model_prim = a_mesh.primitives[prim_index];
		model_prim.start_index = index_offset;
		model_prim.index_count = static_cast<uint32_t>(prim.indices->count);

		MeshMetallic& metallic_info = model_prim.material_data.mesh_metallic;
		metallic_info.base_color_factor.x = prim.material->pbr_metallic_roughness.base_color_factor[0];
		metallic_info.base_color_factor.y = prim.material->pbr_metallic_roughness.base_color_factor[1];
		metallic_info.base_color_factor.z = prim.material->pbr_metallic_roughness.base_color_factor[2];
		metallic_info.base_color_factor.w = prim.material->pbr_metallic_roughness.base_color_factor[3];
		metallic_info.metallic_factor = prim.material->pbr_metallic_roughness.metallic_factor;
		metallic_info.roughness_factor = prim.material->pbr_metallic_roughness.roughness_factor;
		
		model_prim.material_data.material = Material::GetDefaultMaterial(PASS_TYPE::SCENE, MATERIAL_TYPE::MATERIAL_3D);

		if (prim.material->pbr_metallic_roughness.base_color_texture.texture)
		{
			const cgltf_image& image = *prim.material->pbr_metallic_roughness.base_color_texture.texture->image;
			const char* full_image_path = CreateGLTFImagePath(a_temp_arena, image.uri);
			const StringView img = Asset::LoadImageDisk(a_temp_arena, full_image_path);

			model_prim.material_data.base_texture = Asset::FindImageByName(img.c_str())->gpu_image;
		}
		if (prim.material->normal_texture.texture)
		{
			const cgltf_image& image = *prim.material->normal_texture.texture->image;
			const char* full_image_path = CreateGLTFImagePath(a_temp_arena, image.uri);
			const StringView img = Asset::LoadImageDisk(a_temp_arena, full_image_path);
			
			model_prim.material_data.normal_texture = Asset::FindImageByName(img.c_str())->gpu_image;
		}
		{	// get indices
			void* index_data = GetAccessorDataPtr(prim.indices);
			if (prim.indices->component_type == cgltf_component_type_r_32u)
			{
				indices = reinterpret_cast<uint32_t*>(index_data);
			}
			else if (prim.indices->component_type == cgltf_component_type_r_16u)
			{
				const uint16_t* index_data16 = reinterpret_cast<const uint16_t*>(index_data);
				for (size_t i = 0; i < prim.indices->count; i++)
					indices[index_offset + i] = index_data16[i];
				index_offset += static_cast<uint32_t>(prim.indices->count);
			}
			else
				BB_ASSERT(false, "GLTF mesh has an index type that is not supported!");
		}

		for (size_t attrib_index = 0; attrib_index < prim.attributes_count; attrib_index++)
		{
			const cgltf_attribute& attrib = prim.attributes[attrib_index];
			const float* data_pos = reinterpret_cast<float*>(GetAccessorDataPtr(attrib.data));

			switch (attrib.type)
			{
			case cgltf_attribute_type_position:
				for (size_t i = 0; i < attrib.data->count; i++)
				{
					vertices[vertex_pos_offset].position.x = data_pos[0];
					vertices[vertex_pos_offset].position.y = data_pos[1];
					vertices[vertex_pos_offset].position.z = data_pos[2];

					data_pos = reinterpret_cast<const float*>(Pointer::Add(data_pos, attrib.data->stride + attrib.data->buffer_view->stride));
					++vertex_pos_offset;
				}
				break;
			case cgltf_attribute_type_normal:
				for (size_t i = 0; i < attrib.data->count; i++)
				{
					vertices[vertex_normal_offset].normal.x = data_pos[0];
					vertices[vertex_normal_offset].normal.y = data_pos[1];
					vertices[vertex_normal_offset].normal.z = data_pos[2];

					data_pos = reinterpret_cast<const float*>(Pointer::Add(data_pos, attrib.data->stride + attrib.data->buffer_view->stride));
					++vertex_normal_offset;
				}
				break;
			case cgltf_attribute_type_texcoord:
				for (size_t i = 0; i < attrib.data->count; i++)
				{
					vertices[vertex_uv_offset].uv.x = data_pos[0];
					vertices[vertex_uv_offset].uv.y = data_pos[1];

					data_pos = reinterpret_cast<const float*>(Pointer::Add(data_pos, attrib.data->stride + attrib.data->buffer_view->stride));
					++vertex_uv_offset;
				}
				break;
			default:
				break;
			}
			BB_ASSERT(index_count + 1 > index_offset, "overwriting gltf Index Memory!");
			BB_ASSERT(vertex_count + 1 > vertex_pos_offset, "overwriting gltf Vertex Memory!");
			BB_ASSERT(vertex_count + 1 > vertex_normal_offset, "overwriting gltf Vertex Memory!");
			BB_ASSERT(vertex_count + 1 > vertex_uv_offset, "overwriting gltf Vertex Memory!");
		}
	}

	for (size_t i = 0; i < vertex_count; i++)
	{
		vertices[i].color = { 1.f, 1.f, 1.f };
	}

	CreateMeshInfo create_mesh;
	create_mesh.vertices = Slice(vertices, vertex_count);
	create_mesh.indices = Slice(indices, index_count);

	a_mesh.mesh = CreateMesh(create_mesh);
}

struct LoadgltfMeshBatch_params
{
	Slice<cgltf_mesh> cgltf_meshes;
	Slice<Model::Mesh> meshes;
	Threads::Barrier* barrier;
};

static void LoadgltfMeshBatch(MemoryArena& a_temp_arena, void* a_params)
{
	const LoadgltfMeshBatch_params* params = reinterpret_cast<LoadgltfMeshBatch_params*>(a_params);
	BB_ASSERT(params->meshes.size() == params->cgltf_meshes.size(), "cgltf meshes slice size is not equal to meshes slice size");
	for (size_t i = 0; i < params->meshes.size(); i++)
	{
		LoadglTFMesh(a_temp_arena, params->cgltf_meshes[i], params->meshes[i]);
	}
	params->barrier->Signal();
}

static void* cgltf_arena_alloc(void* a_user, cgltf_size a_size)
{
	MemoryArena& arena = *reinterpret_cast<MemoryArena*>(a_user);
	return ArenaAlloc(arena, a_size, 16);
}

static void cgltf_arena_free(void*, void*)
{
	// nothing
}

const StringView Asset::LoadglTFModel(MemoryArena& a_temp_arena, const MeshLoadFromDisk& a_mesh_op)
{
	const AssetHash asset_hash = CreateAssetHash(StringHash(a_mesh_op.path, Memory::StrLength(a_mesh_op.path)), ASSET_TYPE::MODEL);
	
	if (AssetSlot* slot = s_asset_manager->asset_table.find(asset_hash.full_hash))
	{
		return slot->name;
	}

	cgltf_options gltf_option = {};
	gltf_option.memory.alloc_func = cgltf_arena_alloc;
	gltf_option.memory.free_func = cgltf_arena_free;
	gltf_option.memory.user_data = &a_temp_arena;
	cgltf_data* gltf_data = nullptr;

	if (cgltf_parse_file(&gltf_option, a_mesh_op.path, &gltf_data) != cgltf_result_success)
	{
		BB_ASSERT(false, "Failed to load glTF model, cgltf_parse_file.");
		return nullptr;
	}

	cgltf_load_buffers(&gltf_option, gltf_data, a_mesh_op.path);

	if (cgltf_validate(gltf_data) != cgltf_result_success)
	{
		BB_ASSERT(false, "GLTF model validation failed!");
		return nullptr;
	}
	const uint32_t linear_node_count = static_cast<uint32_t>(gltf_data->nodes_count);
	OSAcquireSRWLockWrite(&s_asset_manager->asset_lock);
	// optimize the memory space with one allocation for the entire model, maybe when I convert it to not gltf.
	Model* model = ArenaAllocType(s_asset_manager->asset_arena, Model);
	model->meshes.Init(s_asset_manager->asset_arena, static_cast<uint32_t>(gltf_data->meshes_count));
	model->meshes.resize(model->meshes.capacity());
	for (size_t mesh_index = 0; mesh_index < model->meshes.size(); mesh_index++)
	{
		const cgltf_mesh& cgltf_mesh = gltf_data->meshes[mesh_index];
		Model::Mesh& mesh = model->meshes[mesh_index];

		mesh.primitives.Init(s_asset_manager->asset_arena, static_cast<uint32_t>(cgltf_mesh.primitives_count));
		mesh.primitives.resize(mesh.primitives.capacity());
	}

	model->linear_nodes = ArenaAllocArr(s_asset_manager->asset_arena, Model::Node, linear_node_count);
	model->root_node_count = static_cast<uint32_t>(gltf_data->scene->nodes_count);
	model->root_node_indices = ArenaAllocArr(s_asset_manager->asset_arena, uint32_t, model->root_node_count);
	OSReleaseSRWLockWrite(&s_asset_manager->asset_lock);

	// for every 5 meshes create a thread;
	constexpr size_t TASKS_PER_THREAD = 4;

	const uint32_t thread_count = model->meshes.size() / TASKS_PER_THREAD;

	LoadgltfMeshBatch_params* batches = ArenaAllocArr(a_temp_arena, LoadgltfMeshBatch_params, thread_count);
	Threads::Barrier barrier(thread_count);
	size_t task_index = 0;
	for (uint32_t i = 0; i < thread_count; i++)
	{
		batches[i].barrier = &barrier;
		batches[i].cgltf_meshes = Slice(&gltf_data->meshes[task_index], TASKS_PER_THREAD);
		batches[i].meshes = Slice(&model->meshes[task_index], TASKS_PER_THREAD);
		Threads::StartTaskThread(LoadgltfMeshBatch, &batches[i], L"gltf mesh upload batch");

		task_index += TASKS_PER_THREAD;
	}

	MemoryArenaScope(a_temp_arena)
	{
		for (; task_index < model->meshes.size(); task_index++)
		{
			LoadglTFMesh(a_temp_arena, gltf_data->meshes[task_index], model->meshes[task_index]);
		}
	}

	// check if we have done all the work required.
	barrier.Wait();

	uint32_t current_node = 0;

	for (size_t i = 0; i < gltf_data->scene->nodes_count; i++)
	{
		model->root_node_indices[i] = current_node;
		LoadglTFNode(*gltf_data, *model, current_node);
	}
	cgltf_free(gltf_data);

	AssetString asset_name;
	GetAssetNameFromPath(a_mesh_op.path, asset_name);
	
	AssetSlot asset;
	asset.hash = asset_hash;
	asset.path = FindOrCreateString(a_mesh_op.path).c_str();
	asset.model = model;
	asset.name = FindOrCreateString(asset_name.GetView()).c_str();

	const PathString icon_path = GetIconPathFromAssetName(asset.name);
	if (OSFileExist(icon_path.c_str()))
		asset.icon = LoadIconFromPath(a_temp_arena, icon_path.GetView(), true);
	else
		asset.icon = GetEmptyIconSlot();

	AddElementToAssetTable(asset);
	model->asset_handle = AssetHandle(asset.hash.full_hash);

	return asset.path;
}

const StringView Asset::LoadMeshFromMemory(MemoryArena& a_temp_arena, const MeshLoadFromMemory& a_mesh_op)
{
	// this is all garbage

	const AssetHash asset_hash = CreateAssetHash(StringHash(a_mesh_op.name, Memory::StrLength(a_mesh_op.name)), ASSET_TYPE::MODEL);

	if (AssetSlot* slot = s_asset_manager->asset_table.find(asset_hash.full_hash))
	{
		return slot->name;
	}

	CreateMeshInfo mesh_info;
	mesh_info.vertices = a_mesh_op.vertices;
	mesh_info.indices = a_mesh_op.indices;
	OSAcquireSRWLockWrite(&s_asset_manager->asset_lock);
	//hack shit way, but a single mesh just has one primitive to draw.
	Model* model = ArenaAllocType(s_asset_manager->asset_arena, Model);
	model->linear_nodes = ArenaAllocArr(s_asset_manager->asset_arena, Model::Node, 1);
	model->meshes.Init(s_asset_manager->asset_arena, 1);
	model->meshes[0].primitives.Init(s_asset_manager->asset_arena, 1);
	model->root_node_indices = ArenaAllocArr(s_asset_manager->asset_arena, uint32_t, 1);
	OSReleaseSRWLockWrite(&s_asset_manager->asset_lock);
	Model::Primitive primitive;
	primitive.material_data.base_texture = a_mesh_op.base_color;
	primitive.material_data.normal_texture = GetWhiteTexture();
	primitive.start_index = 0;
	primitive.index_count = static_cast<uint32_t>(a_mesh_op.indices.size());

	Model::Mesh& mesh = model->meshes[0];
	mesh.primitives[0] = primitive;
	mesh.mesh = CreateMesh(mesh_info);

	*model->root_node_indices = 0;

	model->linear_nodes[0] = {};
	model->linear_nodes[0].name = a_mesh_op.name;
	model->linear_nodes[0].child_count = 0;
	model->linear_nodes[0].mesh = &mesh;
	model->linear_nodes[0].scale = float3(1.f, 1.f, 1.f);

	AssetSlot asset;
	asset.hash = asset_hash;
	asset.path = nullptr;
	asset.model = model;
	asset.name = FindOrCreateString(a_mesh_op.name).c_str();

	const PathString icon_path = GetIconPathFromAssetName(asset.name);
	if (OSFileExist(icon_path.c_str()))
		asset.icon = LoadIconFromPath(a_temp_arena, icon_path.GetView(), true);
	else
		asset.icon = GetEmptyIconSlot();

	AddElementToAssetTable(asset);

	model->asset_handle = AssetHandle(asset.hash.full_hash);
	return asset.name;
}

const Model* Asset::FindModelByName(const char* a_name)
{
	AssetHash asset_hash = CreateAssetHash(StringHash(a_name, Memory::StrLength(a_name)), ASSET_TYPE::MODEL);
	if (AssetSlot* slot = s_asset_manager->asset_table.find(asset_hash.full_hash))
	{
		return slot->model;
	}
	return nullptr;
}

const Image* Asset::FindImageByName(const char* a_name)
{
	AssetHash asset_hash = CreateAssetHash(StringHash(a_name, Memory::StrLength(a_name)), ASSET_TYPE::IMAGE);
	if (AssetSlot* slot = s_asset_manager->asset_table.find(asset_hash.full_hash))
	{
		return slot->image;
	}
	return nullptr;
}

void Asset::FreeAsset(const AssetHandle a_asset_handle)
{
	AssetSlot* slot = s_asset_manager->asset_table.find(a_asset_handle.handle);

	// rework this, preferably NO dynamic allocation. 
	switch (slot->hash.type)
	{
	case ASSET_TYPE::MODEL:
		//BBfreeArr(s_asset_manager->allocator, slot->model->linear_nodes);
		//BBfree(s_asset_manager->allocator, slot->model);
		break;
	case ASSET_TYPE::IMAGE:
		//BBfree(s_asset_manager->allocator, slot->image);
		break;
	}

	OSAcquireSRWLockWrite(&s_asset_manager->asset_lock);
	// TODO, erase from the linear array as well
	s_asset_manager->asset_table.erase(a_asset_handle.handle);
	OSReleaseSRWLockWrite(&s_asset_manager->asset_lock);
}

#include "imgui.h"

constexpr size_t ASSET_SEARCH_PATH_SIZE_MAX = 512;

static ThreadTask LoadAssetsASync(const BB::Slice<Asset::AsyncAsset> a_asyn_assets)
{
	// maybe have each thread have it's own memory arena
	MemoryArena load_arena = MemoryArenaCreate();

	LoadAsyncFunc_Params* params = ArenaAllocType(load_arena, LoadAsyncFunc_Params);
	params->assets = ArenaAllocArr(load_arena, Asset::AsyncAsset, a_asyn_assets.size());
	memcpy(params->assets, a_asyn_assets.data(), a_asyn_assets.sizeInBytes());
	params->asset_count = a_asyn_assets.size();
	params->memory_arena = load_arena;

	return Threads::StartTaskThread(LoadAsync_func, params);
}

static void LoadAssetViaSearch()
{
	StackString<ASSET_SEARCH_PATH_SIZE_MAX> search_path;

	if (OSFindFileNameDialogWindow(search_path.data(), search_path.capacity()))
	{
		search_path.RecalculateStringSize();
		const size_t get_extension_pos = search_path.find_last_of('.');
		const size_t get_file_name = search_path.find_last_of('\\');
		BB_ASSERT(get_file_name != size_t(-1), "i fucked up");

		// get asset name
		const char* path_str = Asset::FindOrCreateString(search_path.c_str()).c_str();
		Asset::AsyncAsset asset{};

		// we always load from disk due to the nature of loading an asset via a path
		asset.load_type = Asset::ASYNC_LOAD_TYPE::DISK;

		if (search_path.compare(get_extension_pos, ".gltf"))
		{
			asset.asset_type = Asset::ASYNC_ASSET_TYPE::MODEL;
			asset.mesh_disk.path = path_str;
		}
		else if (search_path.compare(get_extension_pos, ".png") || search_path.compare(get_extension_pos, ".jpg") || search_path.compare(get_extension_pos, ".bmp"))
		{
			asset.asset_type = Asset::ASYNC_ASSET_TYPE::TEXTURE;
			asset.texture_disk.path = path_str;
		}
		else
		{
			BB_ASSERT(false, "NOT SUPPORTED FILE NAME!");
		}

		LoadAssetsASync(Slice(&asset, 1));
	}
}

void Asset::ShowAssetMenu(MemoryArena& a_arena)
{
	if (ImGui::Begin("Asset Menu", nullptr, ImGuiWindowFlags_MenuBar))
	{
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("loader"))
			{
				if (ImGui::MenuItem("load new asset"))
				{
					LoadAssetViaSearch();
				}

				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}

		constexpr int column = 5;
		if (ImGui::BeginTable("assets", column, ImGuiTableFlags_Hideable | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable))
		{
			int current_column_index = 0;
			ImGui::TableNextRow();
			for (size_t i = 0; i < s_asset_manager->linear_asset_table.size(); i++)
			{
				ImGui::PushID(static_cast<int>(i));
				AssetSlot* const slot = s_asset_manager->linear_asset_table[i];
				if (slot == nullptr)
					continue;


				if (current_column_index >= column)
				{
					ImGui::TableNextRow();
					current_column_index = 0;
				}

				ImGui::TableSetColumnIndex(current_column_index++);

				const char* asset_name = slot->name;
				if (!asset_name)
				{
					asset_name = "unnamed";
				}


				if (ImGui::CollapsingHeader(asset_name))
				{
					if (slot->path)
						ImGui::Text("Path: %s", slot->path);
					else
						ImGui::Text("Path: None");

					if (ImGui::Button("Set new Icon"))
					{
						PathString path_search{};
						if (OSFindFileNameDialogWindow(path_search.data(), path_search.capacity()))
						{
							path_search.RecalculateStringSize();
							if (IsPathImage(path_search.GetView()))
							{
								slot->icon = LoadIconFromPath(a_arena, path_search.GetView(), false);
								IconWriteToDisk(slot->icon, GetIconPathFromAssetName(StringView(slot->name)));
							}
						}
					}

					// show icon
					const float icons_texture_width = static_cast<float>(ICON_EXTENT.x * s_asset_manager->icons_storage.max_slots);
					const float slot_size_in_float = static_cast<float>(ICON_EXTENT.x) / icons_texture_width;

					const ImVec2 uv0(slot_size_in_float * static_cast<float>(slot->icon.slot_index), 0);
					const ImVec2 uv1(uv0.x + slot_size_in_float, 1);

					ImGui::Image(s_asset_manager->icons_storage.texture.handle, ImVec2(ICON_EXTENT_F.x, ICON_EXTENT_F.y), uv0, uv1);
				}


				ImGui::PopID();
			}
			ImGui::EndTable();
		}
	}
	ImGui::End();
}
