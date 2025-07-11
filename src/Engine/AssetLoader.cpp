#include "AssetLoader.hpp"

#include "Storage/Hashmap.h"
#include "Storage/Array.h"
#include "BBIntrin.h"
#include "BBImage.hpp"
#include "Math/Math.inl"
#include "Program.h"

#include "shared_common.hlsl.h"
#include "Renderer.hpp"
#include "RendererTypes.hpp"
#include "GPUBuffers.hpp"

#include "MemoryInterfaces.hpp"

#include "BBThreadScheduler.hpp"

#include "Storage/Queue.hpp"

#include "MaterialSystem.hpp"

#include "mikktspace.h"

#include "Engine.hpp"

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

constexpr size_t GPU_TASK_QUEUE_SIZE = 64;

constexpr const char ICON_DIRECTORY[] = "../../resources/icons/";

constexpr const IMAGE_FORMAT ICON_IMAGE_FORMAT = IMAGE_FORMAT::RGBA8_SRGB;

static bool IsPathImage(const StringView a_view)
{
	const size_t extension_pos = a_view.find_last_of('.');

	if (a_view.compare(".png", extension_pos) || a_view.compare(".jpg", extension_pos) || a_view.compare(".bmp", extension_pos))
		return true;
	return false;
}

//crappy hash, don't care for now.
static uint64_t StringHash(const StringView a_view)
{
	uint64_t hash = 5381;

	for (size_t i = 0; i < a_view.size(); i++)
	{
		const uint64_t c = static_cast<uint64_t>(a_view[i]);
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

struct AssetSlot
{
	AssetHash hash;
	PathString path;
	AssetString name;
	uint32_t icon_index;
	union
	{
		Model* model;
		Image* image;
	};

	bool finished_loading;
};

typedef void (*PFN_GPUTaskCallback)(const void* a_params);

struct GPUTask
{
	GPUFenceValue transfer_value;
	PFN_GPUTaskCallback callback;
	void* params;
};

struct UploadDataMesh
{
	RenderCopyBufferRegion vertex_region;
	GPUBuffer vertex_dst_buffer;
	RenderCopyBufferRegion index_region;
	GPUBuffer index_dst_buffer;
};

enum class UPLOAD_TEXTURE_TYPE
{
	WRITE,
	READ
};

struct CreateMeshInfo
{
	ConstSlice<float3> positions;
	ConstSlice<float3> normals;
	ConstSlice<float2> uvs;
	ConstSlice<float4> colors;
	ConstSlice<float3> tangents;
	ConstSlice<uint32_t> indices;
};

struct UploadDataTexture
{
	RImage image;
	UPLOAD_TEXTURE_TYPE upload_type;
	union
	{
		RenderCopyBufferToImageInfo write_info;
		RenderCopyImageToBufferInfo read_info;
	};
	IMAGE_LAYOUT start;
	IMAGE_LAYOUT end;
};

struct GPUUploader
{
	GPUUploadRingAllocator upload_buffer;
	RFence fence;
	std::atomic<uint64_t> next_fence_value;
	MPSCQueue<UploadDataMesh> upload_meshes;
	MPSCQueue<UploadDataTexture> upload_textures;
};

struct WriteImageInfo
{
	ImageInfo image_info;
	IMAGE_FORMAT format;
	const void* pixels;
	bool set_shader_visible;
};

struct ReadImageInfo
{
	ImageInfo image_info;
	IMAGE_LAYOUT layout;

	uint32_t readback_size;
	GPUBuffer readback;
};

struct AssetManager
{
    PathString asset_dir;

	BBRWLock allocator_lock;
	FreelistInterface allocator;

	// asset storage
	BBRWLock asset_lock;
	StaticOL_HashMap<uint64_t, AssetSlot> asset_table;
	StaticArray<AssetSlot*> linear_asset_table;

	MPSCQueue<GPUTask> gpu_tasks_queue;

	struct Preloaded
	{
		struct BasicColorImage
		{
			RImage image;
			RDescriptorIndex index;
		};
		BasicColorImage white;
		BasicColorImage black;
		BasicColorImage red;
		BasicColorImage green;
		BasicColorImage blue;
		BasicColorImage checkerboard;
	} pre_loaded;

	struct IconGigaTexture
	{
		RImage image;
		RDescriptorIndex image_descriptor_index;
		uint32_t max_slots;	// a slot is a texture space that is equal to ICON_EXTENT
		std::atomic<uint32_t> next_index;
	};

	IconGigaTexture icons_storage;
	GPUUploader gpu_uploader;
};
static AssetManager* s_asset_manager;

template<typename T>
static T* AssetAlloc()
{
	BBRWLockScopeWrite(s_asset_manager->allocator_lock);
	return reinterpret_cast<T*>(s_asset_manager->allocator.Alloc(sizeof(T), alignof(T)));
}

template<typename T>
static T* AssetAllocArr(const size_t a_size)
{
	BBRWLockScopeWrite(s_asset_manager->allocator_lock);
	return reinterpret_cast<T*>(s_asset_manager->allocator.Alloc(sizeof(T) * a_size, alignof(T)));
}

static void AssetFree(const void* a_ptr)
{
	BBRWLockScopeWrite(s_asset_manager->allocator_lock);
	s_asset_manager->allocator.Free(a_ptr);
}

static std::atomic<bool> uploading_assets = false;
// TODO, fix this to be lower
constexpr uint32_t MAX_MESH_UPLOAD_QUEUE = 1024;
constexpr uint32_t MAX_TEXTURE_UPLOAD_QUEUE = 1024;

static void UploadAndWaitAssets(MemoryArena& a_thread_arena, void*)
{
	bool expected = false;
	if (uploading_assets.compare_exchange_strong(expected, true))
	{
		CommandPool& cmd_pool = GetTransferCommandPool();
		const RCommandList list = cmd_pool.StartCommandList("upload asset list");
		GPUUploader& uploader = s_asset_manager->gpu_uploader;

		if (!uploader.upload_meshes.IsEmpty())
		{
			MemoryArenaScope(a_thread_arena)
			{
				StaticArray<RenderCopyBufferRegion> vertex_regions{};
				StaticArray<RenderCopyBufferRegion> index_regions{};
				vertex_regions.Init(a_thread_arena, MAX_MESH_UPLOAD_QUEUE);
				index_regions.Init(a_thread_arena, MAX_MESH_UPLOAD_QUEUE);

				UploadDataMesh upload_data;
				while (uploader.upload_meshes.DeQueue(upload_data) && vertex_regions.size() < MAX_MESH_UPLOAD_QUEUE)
				{
					vertex_regions.push_back(upload_data.vertex_region);
					if (upload_data.index_region.size)
						index_regions.push_back(upload_data.index_region);
				}

				CopyToVertexBuffer(list, uploader.upload_buffer.GetBuffer(), vertex_regions.slice());

				if (index_regions.size())
				{
					CopyToIndexBuffer(list, uploader.upload_buffer.GetBuffer(), index_regions.slice());
				}
			}
		}

		if (!uploader.upload_textures.IsEmpty())
		{
			MemoryArenaScope(a_thread_arena)
			{
				uint32_t write_info_count = 0;
				RenderCopyBufferToImageInfo* write_infos = ArenaAllocArr(a_thread_arena, RenderCopyBufferToImageInfo, MAX_TEXTURE_UPLOAD_QUEUE);
				uint32_t read_info_count = 0;
				RenderCopyImageToBufferInfo* read_infos = ArenaAllocArr(a_thread_arena, RenderCopyImageToBufferInfo, MAX_TEXTURE_UPLOAD_QUEUE);
				uint32_t before_trans_count = 0;
				PipelineBarrierImageInfo* before_transition = ArenaAllocArr(a_thread_arena, PipelineBarrierImageInfo, MAX_TEXTURE_UPLOAD_QUEUE);

				size_t index = 0;
				uint16_t base_layer = 0;
				uint16_t layer_count = 0;
				UploadDataTexture upload_data{};
				while (uploader.upload_textures.DeQueue(upload_data) &&
					index++ < MAX_TEXTURE_UPLOAD_QUEUE)
				{
					IMAGE_LAYOUT layout;
					switch (upload_data.upload_type)
					{
					case UPLOAD_TEXTURE_TYPE::WRITE:
						write_infos[write_info_count++] = upload_data.write_info;
						base_layer = upload_data.write_info.dst_image_info.base_array_layer;
						layer_count = upload_data.write_info.dst_image_info.layer_count;
						layout = IMAGE_LAYOUT::COPY_DST;
						break;
					case UPLOAD_TEXTURE_TYPE::READ:
						read_infos[read_info_count++] = upload_data.read_info;
						base_layer = upload_data.read_info.src_image_info.base_array_layer;
						layer_count = upload_data.read_info.src_image_info.layer_count;
						layout = IMAGE_LAYOUT::COPY_SRC;
						break;
					default:
						BB_ASSERT(false, "invalid upload UPLOAD_TEXTURE_TYPE value or unimplemented switch statement");
						break;
					}

					if (upload_data.start != layout)
					{
						PipelineBarrierImageInfo& pi = before_transition[before_trans_count++];
						pi.prev = IMAGE_LAYOUT::NONE;
						pi.next = layout;
						pi.image = upload_data.image;
						pi.layer_count = layer_count;
						pi.level_count = 1;
						pi.base_array_layer = base_layer;
						pi.base_mip_level = 0;
						pi.image_aspect = IMAGE_ASPECT::COLOR;
					}
				}

				PipelineBarrierInfo pipeline_info{};
				pipeline_info.image_barriers = ConstSlice<PipelineBarrierImageInfo>(before_transition, before_trans_count);
				PipelineBarriers(list, pipeline_info);

				for (size_t i = 0; i < write_info_count; i++)
				{
					CopyBufferToImage(list, write_infos[i]);
				}

				for (size_t i = 0; i < read_info_count; i++)
				{
					CopyImageToBuffer(list, read_infos[i]);
				}
			}
		}
		const uint64_t asset_fence_value = uploader.next_fence_value.fetch_add(1);
		cmd_pool.EndCommandList(list);
		uint64_t mock_fence;	// TODO, remove this
		bool success = ExecuteTransferCommands(Slice(&cmd_pool, 1), &uploader.fence, &asset_fence_value, 1, mock_fence);
		BB_ASSERT(success, "failed to execute transfer commands");
	}
	else
	{
		while (uploading_assets) {}
	}

	uploading_assets = false;
}

static GPUFenceValue CreateMesh(MemoryArena& a_temp_arena, const CreateMeshInfo& a_create_info, Mesh& a_out_mesh)
{
	GPUUploader& uploader = s_asset_manager->gpu_uploader;

	const size_t vertex_buffer_size = a_create_info.positions.sizeInBytes() + a_create_info.normals.sizeInBytes() + a_create_info.uvs.sizeInBytes() + a_create_info.colors.sizeInBytes() + a_create_info.tangents.sizeInBytes();

	auto memcpy_and_advance = [](const GPUUploadRingAllocator& a_buffer, const size_t a_dst_offset, const void* a_src_data, const size_t a_src_size)
		{
			const bool success = a_buffer.MemcpyIntoBuffer(a_dst_offset, a_src_data, a_src_size);
			BB_ASSERT(success, "failed to memcpy mesh data into gpu visible buffer");
			return a_dst_offset + a_src_size;
		};

	const GPUFenceValue fence_value = GPUFenceValue(uploader.next_fence_value.load());
	const size_t vertex_start_offset = uploader.upload_buffer.AllocateUploadMemory(vertex_buffer_size + a_create_info.indices.sizeInBytes(), fence_value);
	if (vertex_start_offset == size_t(-1))
	{
		UploadAndWaitAssets(a_temp_arena, nullptr);
		return CreateMesh(a_temp_arena, a_create_info, a_out_mesh);
	}

	const size_t normal_offset = memcpy_and_advance(uploader.upload_buffer, vertex_start_offset, a_create_info.positions.data(), a_create_info.positions.sizeInBytes());
	const size_t uv_offset = memcpy_and_advance(uploader.upload_buffer, normal_offset, a_create_info.normals.data(), a_create_info.normals.sizeInBytes());
	const size_t color_offset = memcpy_and_advance(uploader.upload_buffer, uv_offset, a_create_info.uvs.data(), a_create_info.uvs.sizeInBytes());
	const size_t tangent_offset = memcpy_and_advance(uploader.upload_buffer, color_offset, a_create_info.colors.data(), a_create_info.colors.sizeInBytes());
	const size_t index_offset = memcpy_and_advance(uploader.upload_buffer, tangent_offset, a_create_info.tangents.data(), a_create_info.tangents.sizeInBytes());

	// now the indices
	memcpy_and_advance(uploader.upload_buffer, index_offset, a_create_info.indices.data(), a_create_info.indices.sizeInBytes());

	const GPUBufferView vertex_buffer = AllocateFromVertexBuffer(vertex_buffer_size);
	const GPUBufferView index_buffer = a_create_info.indices.size() ? AllocateFromIndexBuffer(a_create_info.indices.sizeInBytes()) : GPUBufferView();

	UploadDataMesh task{};
	task.vertex_region.size = vertex_buffer.size;
	task.vertex_region.dst_offset = vertex_buffer.offset;
	task.vertex_region.src_offset = vertex_start_offset;
	task.index_region.size = index_buffer.size;
	task.index_region.dst_offset = index_buffer.offset;
	task.index_region.src_offset = index_offset;
	const bool success = uploader.upload_meshes.EnQueue(task);
	BB_ASSERT(success, "failed to add mesh to uploadmesh tasks");

	Mesh mesh{};
	mesh.vertex_position_offset = vertex_buffer.offset;
	mesh.vertex_normal_offset = vertex_buffer.offset + normal_offset - vertex_start_offset;
	mesh.vertex_uv_offset = vertex_buffer.offset + uv_offset - vertex_start_offset;
	mesh.vertex_color_offset = vertex_buffer.offset + color_offset - vertex_start_offset;
	mesh.vertex_tangent_offset = vertex_buffer.offset + tangent_offset - vertex_start_offset;
	mesh.index_buffer_offset = index_buffer.offset;

	a_out_mesh = mesh;
	return fence_value;
}

static GPUFenceValue WriteTexture(MemoryArena& a_temp_arena, const WriteImageInfo& a_write_info)
{
	BB_ASSERT(a_write_info.image_info.extent.x != 0 && a_write_info.image_info.extent.y != 0, "one extent value is 0");
	GPUUploader& uploader = s_asset_manager->gpu_uploader;

	auto format_byte_size = [](const IMAGE_FORMAT a_format) -> size_t
		{
			switch (a_format)
			{
			case IMAGE_FORMAT::RGBA16_UNORM:
			case IMAGE_FORMAT::RGBA16_SFLOAT:
				return 8;
			case IMAGE_FORMAT::RGBA8_SRGB:
			case IMAGE_FORMAT::RGBA8_UNORM:
				return 4;
			case IMAGE_FORMAT::RGB8_SRGB:
				return 3;
			case IMAGE_FORMAT::A8_UNORM:
				return 1;
			default:
				BB_ASSERT(false, "Unsupported bit_count for upload image");
				return 4;
			}
		};

	const size_t write_size = format_byte_size(a_write_info.format) * a_write_info.image_info.extent.x * a_write_info.image_info.extent.y;
	const GPUFenceValue fence_value = GPUFenceValue(uploader.next_fence_value.load());
	const size_t upload_start = uploader.upload_buffer.AllocateUploadMemory(write_size, fence_value);
	if (upload_start == size_t(-1))
	{ 
		UploadAndWaitAssets(a_temp_arena, nullptr);
		return WriteTexture(a_temp_arena, a_write_info);
	}
	bool success = uploader.upload_buffer.MemcpyIntoBuffer(upload_start, a_write_info.pixels, write_size);
	BB_ASSERT(success, "failed to upload data into upload buffer");

	RenderCopyBufferToImageInfo buffer_to_image;
	buffer_to_image.src_buffer = uploader.upload_buffer.GetBuffer();
	buffer_to_image.src_offset = upload_start;

	buffer_to_image.dst_image = a_write_info.image_info.image;
	buffer_to_image.dst_image_info.extent.x = a_write_info.image_info.extent.x;
	buffer_to_image.dst_image_info.extent.y = a_write_info.image_info.extent.y;
	buffer_to_image.dst_image_info.extent.z = 1;
	buffer_to_image.dst_image_info.offset.x = a_write_info.image_info.offset.x;
	buffer_to_image.dst_image_info.offset.y = a_write_info.image_info.offset.y;
	buffer_to_image.dst_image_info.offset.z = 0;
	buffer_to_image.dst_image_info.mip_level = a_write_info.image_info.mip_level;
	buffer_to_image.dst_image_info.layer_count = a_write_info.image_info.array_layers;
	buffer_to_image.dst_image_info.base_array_layer = a_write_info.image_info.base_array_layer;
	buffer_to_image.dst_aspects = IMAGE_ASPECT::COLOR;

	UploadDataTexture upload_texture{};
	upload_texture.image = a_write_info.image_info.image;
	upload_texture.upload_type = UPLOAD_TEXTURE_TYPE::WRITE;
	upload_texture.write_info = buffer_to_image;
	upload_texture.start = IMAGE_LAYOUT::NONE;
	upload_texture.end = a_write_info.set_shader_visible ? IMAGE_LAYOUT::RO_FRAGMENT : IMAGE_LAYOUT::COPY_DST;
	success = uploader.upload_textures.EnQueue(upload_texture);
	BB_ASSERT(success, "failed to add mesh to upload_textures tasks");

	return fence_value;
}

static GPUFenceValue ReadTexture(const ReadImageInfo& a_image_info)
{
	// TODO, check the image's pixel size
	constexpr size_t SCREENSHOT_IMAGE_PIXEL_BYTE_SIZE = 4;

	const size_t image_size =
		static_cast<size_t>(a_image_info.image_info.extent.x) *
		static_cast<size_t>(a_image_info.image_info.extent.y) *
		SCREENSHOT_IMAGE_PIXEL_BYTE_SIZE;

	BB_ASSERT(image_size <= a_image_info.readback_size, "readback buffer too small");

	RenderCopyImageToBufferInfo image_to_buffer{};
	image_to_buffer.dst_buffer = a_image_info.readback;
	image_to_buffer.dst_offset = 0; // change this if i have a global readback buffer thing

	image_to_buffer.src_image = a_image_info.image_info.image;
	image_to_buffer.src_aspects = IMAGE_ASPECT::COLOR;
	image_to_buffer.src_image_info.extent.x = a_image_info.image_info.extent.x;
	image_to_buffer.src_image_info.extent.y = a_image_info.image_info.extent.y;
	image_to_buffer.src_image_info.extent.z = 1;
	image_to_buffer.src_image_info.offset.x = a_image_info.image_info.offset.x;
	image_to_buffer.src_image_info.offset.y = a_image_info.image_info.offset.y;
	image_to_buffer.src_image_info.offset.z = 0;
	image_to_buffer.src_image_info.mip_level = a_image_info.image_info.mip_level;
	image_to_buffer.src_image_info.layer_count = a_image_info.image_info.array_layers;
	image_to_buffer.src_image_info.base_array_layer = a_image_info.image_info.base_array_layer;

	GPUUploader& uploader = s_asset_manager->gpu_uploader;
	const GPUFenceValue fence_value = GPUFenceValue(uploader.next_fence_value.load());
	UploadDataTexture upload_texture{};
	upload_texture.image = a_image_info.image_info.image;
	upload_texture.upload_type = UPLOAD_TEXTURE_TYPE::READ;
	upload_texture.read_info = image_to_buffer;
	upload_texture.start = a_image_info.layout;
	upload_texture.end = a_image_info.layout;
	const bool success = uploader.upload_textures.EnQueue(upload_texture);
	BB_ASSERT(success, "failed to add mesh to upload_textures tasks");

	return fence_value;
}

static void CreateBasicColorImage(MemoryArena& a_temp_arena, RImage& a_image, RDescriptorIndex& a_index, const char* a_name, const uint32_t a_width_height, const uint32_t* a_colors)
{
	ImageCreateInfo image_info;
	image_info.name = a_name;
	image_info.width = a_width_height;
	image_info.height = a_width_height;
	image_info.depth = 1;
	image_info.array_layers = 1;
	image_info.mip_levels = 1;
	image_info.type = IMAGE_TYPE::TYPE_2D;
	image_info.use_optimal_tiling = true;
	image_info.format = IMAGE_FORMAT::RGBA8_SRGB;
	image_info.usage = IMAGE_USAGE::TEXTURE;
	image_info.is_cube_map = false;
	a_image = CreateImage(image_info);

	ImageViewCreateInfo view_info;
	view_info.name = a_name;
	view_info.base_array_layer = 0;
	view_info.mip_levels = 1;
	view_info.array_layers = 1;
	view_info.base_mip_level = 0;
	view_info.type = IMAGE_VIEW_TYPE::TYPE_2D;
	view_info.format = IMAGE_FORMAT::RGBA8_SRGB;
	view_info.image = a_image;
	view_info.aspects = IMAGE_ASPECT::COLOR;
	a_index = CreateImageView(view_info);

	WriteImageInfo write_info;
	write_info.image_info.image = a_image;
	write_info.image_info.extent = uint2(a_width_height, a_width_height);
	write_info.image_info.offset = int2(0, 0);
	write_info.image_info.array_layers = 1;
	write_info.image_info.mip_level = 0;
	write_info.image_info.base_array_layer = 0;
	write_info.format = IMAGE_FORMAT::RGBA8_SRGB;
	write_info.pixels = a_colors;
	write_info.set_shader_visible = true;
	WriteTexture(a_temp_arena, write_info);
}

template<typename T>
static bool AddGPUTask(const PFN_GPUTaskCallback a_callback, const T& a_params, const GPUFenceValue a_fence_value)
{
	if (s_asset_manager->gpu_tasks_queue.IsFull())
		return false;
	GPUTask task;
	task.transfer_value = a_fence_value;
	task.callback = a_callback;
	task.params = reinterpret_cast<void*>(AssetAlloc<T>());
	*reinterpret_cast<T*>(task.params) = a_params;
	s_asset_manager->gpu_tasks_queue.EnQueue(task);
	return true;
}

static void ExecuteGPUTasks()
{
	const GPUFenceValue fence_value = GetCurrentFenceValue(s_asset_manager->gpu_uploader.fence);

	GPUTask task;
	bool success = s_asset_manager->gpu_tasks_queue.PeekTail(task);
	while (success && task.transfer_value <= fence_value)
	{
		task.callback(task.params);
		// free the param memory
		AssetFree(task.params);
		success = s_asset_manager->gpu_tasks_queue.DeQueueNoGet();
		BB_ASSERT(success, "failed to dequeue the gputask which should be impossible to fail");
		success = s_asset_manager->gpu_tasks_queue.PeekTail(task);
	}
}

static inline AssetString GetAssetIconName(const StringView& a_asset_name)
{
	AssetString icon_name(a_asset_name.c_str(), a_asset_name.size());
	icon_name.append(" icon");
	return icon_name;
}

static inline void GetAssetNameFromPath(const PathString& a_path, AssetString& a_asset_name)
{
	size_t name_start = a_path.find_last_of_directory_slash();
	if (name_start == -1) // path already has the name
		name_start = 0;
	const size_t name_end = a_path.find_last_of_extension_seperator();
	BB_ASSERT(name_end != size_t(-1), "file has no file extension name");
	a_asset_name.append(&a_path[name_start + 1], name_end - name_start - 1);
}

static inline PathString GetIconPathFromAssetName(const StringView& a_asset_name)
{
	PathString icon_path(ICON_DIRECTORY);
	icon_path.AddPathNoSlash(GetAssetIconName(a_asset_name).GetView());
	icon_path.AddPathNoSlash(".png");
	return icon_path;
}

static inline uint32_t GetNextIconIndex()
{
	const uint32_t slot = s_asset_manager->icons_storage.next_index.fetch_add(1, std::memory_order_relaxed);
	BB_ASSERT(slot <= s_asset_manager->icons_storage.max_slots, "icon storage full");
	return slot;
}

static inline void* ResizeImage(MemoryArena& a_arena, const void* a_src_pixels, const int a_src_width, const int a_src_height, const int a_dst_width, const int a_dst_height)
{
	unsigned char* icon_pixels = ArenaAllocArr(a_arena, unsigned char, ICON_EXTENT.x * ICON_EXTENT.y * 4);

	BB_ASSERT(stbir_resize_uint8_srgb(
		reinterpret_cast<const unsigned char*>(a_src_pixels), a_src_width, a_src_height, 0,
		icon_pixels, a_dst_width, a_dst_height, 0, STBIR_RGBA), "failed to resize image using stibr");

	return icon_pixels;
}

struct WriteToDisk_params
{
	GPUBuffer readback;
	uint2 image_extent;
	PathString write_path;
};

static inline void WriteToDisk_impl(const void* a_params)
{
	const WriteToDisk_params* params = reinterpret_cast<const WriteToDisk_params*>(a_params);

	const void* mapped = MapGPUBuffer(params->readback);
	const bool success = Asset::WriteImage(params->write_path.c_str(), params->image_extent, 4, mapped);
	BB_WARNING(success, "failed to write image to disk", WarningType::MEDIUM);
	UnmapGPUBuffer(params->readback);
	FreeGPUBuffer(params->readback);
}
#pragma region icon
static inline bool IconWriteToDisk(const uint32_t a_icon_index, const PathString& a_write_path)
{
	const uint32_t readback_size = ICON_EXTENT.x * ICON_EXTENT.y * 4;

	GPUBufferCreateInfo readback_buff;
	readback_buff.name = "icon readback";
	readback_buff.size = static_cast<uint64_t>(readback_size) * 4; // 4 color channels
	readback_buff.host_writable = true;
	readback_buff.type = BUFFER_TYPE::READBACK;
	const GPUBuffer readback = CreateGPUBuffer(readback_buff);

	const int2 read_offset(static_cast<int>(a_icon_index * ICON_EXTENT.x), 0);

	ReadImageInfo read_info;
	read_info.image_info.image = s_asset_manager->icons_storage.image;
	read_info.image_info.extent = ICON_EXTENT;
	read_info.image_info.offset = read_offset;
	read_info.image_info.array_layers = 1;
	read_info.image_info.base_array_layer = 0;
	read_info.image_info.mip_level = 0;
	read_info.layout = IMAGE_LAYOUT::COPY_DST;
	read_info.readback_size = readback_size;
	read_info.readback = readback;
	const GPUFenceValue fence_value = ReadTexture(read_info);

	WriteToDisk_params params;
	params.readback = readback;
	params.image_extent = uint2(ICON_EXTENT.x, ICON_EXTENT.y);
	params.write_path = a_write_path;
	
	return AddGPUTask(WriteToDisk_impl, params, fence_value);
}

static inline uint32_t LoadIconFromPath(MemoryArena& a_temp_arena, const StringView a_icon_path, const bool a_set_icons_shader_visible)
{
	int width = 0, height = 0, channels = 0;
	stbi_uc* pixels = stbi_load(a_icon_path.c_str(), &width, &height, &channels, 4);

	const void* write_pixels = pixels;

	if (!pixels || width != static_cast<int>(ICON_EXTENT.x) || height != static_cast<int>(ICON_EXTENT.y))
	{
		write_pixels = ResizeImage(a_temp_arena, pixels, width, height, ICON_EXTENT.x, ICON_EXTENT.y);
	}

	const uint32_t index = GetNextIconIndex();

	WriteImageInfo write_icon_info;
	write_icon_info.format = ICON_IMAGE_FORMAT;
	write_icon_info.image_info.image = s_asset_manager->icons_storage.image;
	write_icon_info.image_info.extent = ICON_EXTENT;
	write_icon_info.image_info.offset = int2(static_cast<int>(index * ICON_EXTENT.x), 0);
	write_icon_info.image_info.mip_level = 0;
	write_icon_info.image_info.array_layers = 1;
	write_icon_info.image_info.base_array_layer = 0;
	write_icon_info.pixels = write_pixels;
	write_icon_info.set_shader_visible = a_set_icons_shader_visible;
	WriteTexture(a_temp_arena, write_icon_info);

	return index;
}

static inline uint32_t LoadIconFromPixels(MemoryArena& a_temp_arena, const void* a_pixels, const bool a_set_icons_shader_visible)
{
	const uint32_t index = GetNextIconIndex();

	WriteImageInfo write_icon_info;
	write_icon_info.image_info.image = s_asset_manager->icons_storage.image;
	write_icon_info.image_info.extent = ICON_EXTENT;
	write_icon_info.image_info.offset = int2(static_cast<int>(index * ICON_EXTENT.x), 0);
	write_icon_info.image_info.mip_level = 0;
	write_icon_info.image_info.array_layers = 1;
	write_icon_info.image_info.base_array_layer = 0;
	write_icon_info.format = ICON_IMAGE_FORMAT;
	write_icon_info.pixels = a_pixels;
	write_icon_info.set_shader_visible = a_set_icons_shader_visible;
	WriteTexture(a_temp_arena, write_icon_info);

	return index;
}
#pragma endregion icon

static inline AssetSlot& FindElementOrCreateElement(const AssetHash a_hash, bool& a_out_found)
{
	const BBRWLockScopeWrite lock(s_asset_manager->asset_lock);
	if (AssetSlot* found_slot = s_asset_manager->asset_table.find(a_hash.full_hash))
	{
		while (!found_slot->finished_loading) {}
		a_out_found = true;
		return *found_slot;
	}

	AssetSlot* slot = s_asset_manager->asset_table.insert(a_hash.full_hash, AssetSlot());
	BB_ASSERT(slot, "asset slot invalid, asset table is full?");
	s_asset_manager->linear_asset_table.push_back(slot);
	slot->finished_loading = false;

	switch (a_hash.type)
	{
	case ASSET_TYPE::MODEL:
		slot->model = AssetAlloc<Model>();
		break;
	case ASSET_TYPE::IMAGE:
		slot->image = AssetAlloc<Image>();
		break;
	default:
		BB_ASSERT(false, "unknown ASSET_TYPE");
		break;
	}

	a_out_found = false;
	return *slot;
}

using namespace BB;

void Asset::InitializeAssetManager(MemoryArena& a_arena, const AssetManagerInitInfo& a_init_info)
{
	BB_ASSERT(!s_asset_manager, "Asset Manager already initialized");
	s_asset_manager = ArenaAllocType(a_arena, AssetManager);
	s_asset_manager->allocator_lock = OSCreateRWLock();
	s_asset_manager->allocator.Initialize(a_arena, mbSize * 64);

	s_asset_manager->asset_lock = OSCreateRWLock();
	s_asset_manager->asset_table.Init(a_arena, a_init_info.asset_count);
	s_asset_manager->linear_asset_table.Init(a_arena, a_init_info.asset_count);

	s_asset_manager->gpu_tasks_queue.Init(a_arena, GPU_TASK_QUEUE_SIZE);

	s_asset_manager->icons_storage.max_slots = a_init_info.asset_count;
	s_asset_manager->icons_storage.next_index = 0;
    
    s_asset_manager->asset_dir = GetRootPath();
    s_asset_manager->asset_dir.AddPath(StringView("resources"));

	ImageCreateInfo icons_image_info;
	icons_image_info.name = "icon mega image";
	icons_image_info.width = ICON_EXTENT.x * s_asset_manager->icons_storage.max_slots;
	icons_image_info.height = ICON_EXTENT.y;
	icons_image_info.depth = 1;
	icons_image_info.mip_levels = 1;
	icons_image_info.array_layers = 1;
	icons_image_info.type = IMAGE_TYPE::TYPE_2D;
	icons_image_info.usage = IMAGE_USAGE::TEXTURE;
	icons_image_info.format = ICON_IMAGE_FORMAT;
	icons_image_info.use_optimal_tiling = true;
	icons_image_info.is_cube_map = false;

	s_asset_manager->icons_storage.image = CreateImage(icons_image_info);

	ImageViewCreateInfo icons_view_info;
	icons_view_info.name = "icon mega image view";
	icons_view_info.image = s_asset_manager->icons_storage.image;
	icons_view_info.array_layers = 1;
	icons_view_info.base_array_layer = 0;
	icons_view_info.mip_levels = 1;
	icons_view_info.base_mip_level = 0;
	icons_view_info.format = IMAGE_FORMAT::RGBA8_SRGB;
	icons_view_info.type = IMAGE_VIEW_TYPE::TYPE_2D;
	icons_view_info.aspects = IMAGE_ASPECT::COLOR;
	s_asset_manager->icons_storage.image_descriptor_index = CreateImageView(icons_view_info);

	// create some directories for files we are going to write
	if (!OSDirectoryExist(ICON_DIRECTORY))
		BB_ASSERT(OSCreateDirectory(ICON_DIRECTORY), "failed to create ICON directory");


	{	// do asset upload allocator here
		s_asset_manager->gpu_uploader.upload_meshes.Init(a_arena, MAX_MESH_UPLOAD_QUEUE);
		s_asset_manager->gpu_uploader.upload_textures.Init(a_arena, MAX_TEXTURE_UPLOAD_QUEUE);
		s_asset_manager->gpu_uploader.fence = CreateFence(0, "asset upload fence");
		s_asset_manager->gpu_uploader.upload_buffer.Init(a_arena,
			a_init_info.asset_upload_buffer_size,
			s_asset_manager->gpu_uploader.fence,
			"asset upload buffer");
		s_asset_manager->gpu_uploader.next_fence_value = 1;
	}

	MemoryArenaScope(a_arena)
	{ 
		//some basic colors
		uint32_t white = UINT32_MAX;
		CreateBasicColorImage(a_arena, s_asset_manager->pre_loaded.white.image, s_asset_manager->pre_loaded.white.index, "white", 1, &white);
		uint32_t black = 0x000000FF;
		CreateBasicColorImage(a_arena, s_asset_manager->pre_loaded.black.image, s_asset_manager->pre_loaded.black.index, "black", 1, &black);
		uint32_t red = 0xFF0000FF;
		CreateBasicColorImage(a_arena, s_asset_manager->pre_loaded.red.image, s_asset_manager->pre_loaded.red.index, "red", 1, &red);
		uint32_t green = 0x00FF00FF;
		CreateBasicColorImage(a_arena, s_asset_manager->pre_loaded.green.image, s_asset_manager->pre_loaded.green.index, "green", 1, &green);
		uint32_t blue = 0x0000FFFF;
		CreateBasicColorImage(a_arena, s_asset_manager->pre_loaded.blue.image, s_asset_manager->pre_loaded.blue.index, "blue", 1, &blue);
		uint32_t checkerboard[4] = {white, black, black, white};
		CreateBasicColorImage(a_arena, s_asset_manager->pre_loaded.blue.image, s_asset_manager->pre_loaded.checkerboard.index, "checkerboard", 2, checkerboard);
	}
}

void Asset::Update()
{
	if (!uploading_assets)
	{
		Threads::StartTaskThread(UploadAndWaitAssets, L"upload assets");
	}
	ExecuteGPUTasks();
}

struct LoadAsyncFunc_Params
{
	size_t asset_count;
	Asset::AsyncAsset* assets;
};

static void LoadAsync_func(MemoryArena& a_arena, void* a_param)
{
	LoadAsyncFunc_Params* params = reinterpret_cast<LoadAsyncFunc_Params*>(a_param);

	LoadAssets(a_arena, Slice(params->assets, params->asset_count));
}

ThreadTask Asset::LoadAssetsASync(MemoryArenaTemp a_temp_arena, const BB::Slice<Asset::AsyncAsset> a_asyn_assets)
{
	const size_t alloc_size = sizeof(size_t) * a_asyn_assets.sizeInBytes();
	LoadAsyncFunc_Params* params = reinterpret_cast<LoadAsyncFunc_Params*>(ArenaAlloc(a_temp_arena, alloc_size, 8));
	params->asset_count = a_asyn_assets.size();
	params->assets = reinterpret_cast<Asset::AsyncAsset*>(Pointer::Add(params, sizeof(size_t)));
	memcpy(params->assets, a_asyn_assets.data(), a_asyn_assets.sizeInBytes());

	return Threads::StartTaskThread(LoadAsync_func, params, alloc_size);
}

void Asset::LoadAssets(MemoryArena& a_temp_arena, const Slice<AsyncAsset> a_asyn_assets)
{
	for (size_t i = 0; i < a_asyn_assets.size(); i++)
	{
		const AsyncAsset& task = a_asyn_assets[i];
		switch (task.asset_type)
		{
		case ASYNC_ASSET_TYPE::MODEL:
		{
			switch (task.load_type)
			{
			case ASYNC_LOAD_TYPE::DISK:
				LoadglTFModel(a_temp_arena, task.mesh_disk);
				break;
			case ASYNC_LOAD_TYPE::MEMORY:
				LoadMeshFromMemory(a_temp_arena, task.mesh_memory);
				break;
			default:
				BB_ASSERT(false, "default hit while it shouldn't");
				break;
			}
			break;
		}
		case ASYNC_ASSET_TYPE::TEXTURE:
		{
			switch (task.load_type)
			{
			case ASYNC_LOAD_TYPE::DISK:
				LoadImageDisk(a_temp_arena, task.texture_disk.path, task.texture_disk.format);
				break;
			case ASYNC_LOAD_TYPE::MEMORY:
				LoadImageMemory(a_temp_arena, task.texture_memory);
				break;
			default:
				BB_ASSERT(false, "default hit while it shouldn't");
				break;
			}
		}
		break;
		default:
			BB_ASSERT(false, "default hit while it shouldn't");
			break;
		}
	}
}

const PathString& Asset::GetAssetPath()
{
    return s_asset_manager->asset_dir;
}

static PathString CreateTexturePath(const StringView& a_image_path)
{
	PathString path = s_asset_manager->asset_dir;
	path.AddPath("textures");
	path.AddPathNoSlash(a_image_path);
	return path;
}

static PathString CreateModelPath(const StringView& a_model_path)
{
	PathString path = s_asset_manager->asset_dir;
	path.AddPath("models");
	path.AddPathNoSlash(a_model_path);
	return path;
}

static inline void CreateImage_func(const StringView& a_name, const uint32_t a_width, const uint32_t a_height, const uint16_t a_array_layers, const IMAGE_FORMAT a_format, const IMAGE_VIEW_TYPE a_view_type, RImage& a_out_image, RDescriptorIndex& a_out_index)
{
	ImageCreateInfo create_image_info;
	create_image_info.name = a_name.c_str();
	create_image_info.width = static_cast<uint32_t>(a_width);
	create_image_info.height = static_cast<uint32_t>(a_height);
	create_image_info.depth = 1;
	create_image_info.array_layers = a_array_layers;
	create_image_info.mip_levels = 1;
	create_image_info.type = IMAGE_TYPE::TYPE_2D;
	create_image_info.format = a_format;
	create_image_info.usage = IMAGE_USAGE::TEXTURE;
	create_image_info.use_optimal_tiling = true;
	create_image_info.is_cube_map = a_view_type == IMAGE_VIEW_TYPE::CUBE ? true : false;
	a_out_image = CreateImage(create_image_info);

	ImageViewCreateInfo create_view_info;
	create_view_info.name = a_name.c_str();
	create_view_info.image = a_out_image;
	create_view_info.base_array_layer = 0;
	create_view_info.array_layers = a_array_layers;
	create_view_info.mip_levels = 1;
	create_view_info.base_mip_level = 0;
	create_view_info.type = a_view_type;
	create_view_info.format = a_format;
	create_view_info.aspects = IMAGE_ASPECT::COLOR;
	a_out_index = CreateImageView(create_view_info);
}

const Image& Asset::LoadImageDisk(MemoryArena& a_temp_arena, const StringView& a_path, const IMAGE_FORMAT a_format)
{
	const AssetHash path_hash = CreateAssetHash(StringHash(a_path), ASSET_TYPE::IMAGE);
	bool exists = false;
	AssetSlot& asset = FindElementOrCreateElement(path_hash, exists);
	if (exists)
		return *asset.image;
	GetAssetNameFromPath(a_path, asset.name);

	int width = 0, height = 0, channels = 0;
	stbi_uc* pixels = stbi_load(CreateTexturePath(a_path).c_str(), &width, &height, &channels, 4);
	RImage gpu_image;
	RDescriptorIndex descriptor_index;
	const IMAGE_FORMAT format = a_format;
	const uint32_t uwidth = static_cast<uint32_t>(width);
	const uint32_t uheight = static_cast<uint32_t>(height);
	CreateImage_func(asset.name.GetView(), uwidth, uheight, 1, format, IMAGE_VIEW_TYPE::TYPE_2D, gpu_image, descriptor_index);

	WriteImageInfo write_info{};
	write_info.image_info.image = gpu_image;
	write_info.image_info.extent = { uwidth, uheight };
	write_info.image_info.mip_level = 0;
	write_info.image_info.array_layers = 1;
	write_info.image_info.base_array_layer = 0;
	write_info.format = format;
	write_info.pixels = pixels;
	write_info.set_shader_visible = true;
	WriteTexture(a_temp_arena, write_info);
	
	asset.hash = path_hash;
	asset.path = a_path;

	asset.image->width = uwidth;
	asset.image->height = uheight;
	asset.image->array_layers = 1;
	asset.image->gpu_image = gpu_image;
	asset.image->descriptor_index = descriptor_index;
	asset.image->asset_handle = AssetHandle(asset.hash.full_hash);

	const PathString icon_path = GetIconPathFromAssetName(asset.name.GetView());
	if (OSFileExist(icon_path.c_str()))
	{
		asset.icon_index = LoadIconFromPath(a_temp_arena, icon_path.GetView(), true);
	}
	else
	{
		const void* icon_write = pixels;
		if (uwidth != ICON_EXTENT.x || uheight != ICON_EXTENT.y)
		{
			icon_write = ResizeImage(a_temp_arena, pixels, width, height, static_cast<int>(ICON_EXTENT.x), static_cast<int>(ICON_EXTENT.y));
		}
		asset.icon_index = LoadIconFromPixels(a_temp_arena, icon_write, true);
		WriteImage(icon_path.GetView(), uint2(ICON_EXTENT.x, ICON_EXTENT.y), 4, icon_write);
	}

	STBI_FREE(pixels);
	asset.finished_loading = true;
	return *asset.image;
}

const Image& Asset::LoadImageArrayDisk(MemoryArena& a_temp_arena, const StringView& a_name, const ConstSlice<StringView> a_paths, const IMAGE_FORMAT a_format, const bool a_is_cube_map)
{
	BB_ASSERT(a_paths.size() != 0, "no paths are given");
	if (a_is_cube_map)
	{
		BB_ASSERT(a_paths.size() == 6, "trying to create a cubemap but not sending 6 paths");
	}
	uint64_t hash = 0;
	for (size_t i = 0; i < a_paths.size(); i++)
	{
		hash += StringHash(a_paths[i]);
	}
	const AssetHash path_hash = CreateAssetHash(hash, ASSET_TYPE::IMAGE);
	bool exists = false;
	AssetSlot& asset = FindElementOrCreateElement(path_hash, exists);
	if (exists)
		return *asset.image;
	asset.name = a_name;

	int width = 0, height = 0, channels = 0;
	stbi_uc* pixels = stbi_load(CreateTexturePath(a_paths[0]).c_str(), &width, &height, &channels, 4);
	RImage gpu_image;
	RDescriptorIndex descriptor_index;
	const uint32_t uwidth = static_cast<uint32_t>(width);
	const uint32_t uheight = static_cast<uint32_t>(height);
	const uint32_t array_layers = static_cast<uint32_t>(a_paths.size());
	CreateImage_func(asset.name.GetView(), uwidth, uheight, static_cast<uint16_t>(array_layers), a_format, a_is_cube_map ? IMAGE_VIEW_TYPE::CUBE : IMAGE_VIEW_TYPE::TYPE_2D_ARRAY, gpu_image, descriptor_index);

	WriteImageInfo write_info{};
	write_info.image_info.image = gpu_image;
	write_info.image_info.extent = { uwidth, uheight };
	write_info.image_info.mip_level = 0;
	write_info.image_info.array_layers = 1;
	write_info.image_info.base_array_layer = 0;
	write_info.format = a_format;
	write_info.pixels = pixels;
	write_info.set_shader_visible = true;
	WriteTexture(a_temp_arena, write_info);

	for (uint32_t i = 1; i < array_layers; i++)
	{
		STBI_FREE(pixels);
		pixels = stbi_load(CreateTexturePath(a_paths[i]).c_str(), &width, &height, &channels, 4);

		BB_ASSERT(uwidth == static_cast<uint32_t>(width) && uheight == static_cast<uint32_t>(height), "image array are not the same dimensions");

		write_info.image_info.mip_level = 0;
		write_info.image_info.array_layers = 1;
		write_info.image_info.base_array_layer = static_cast<uint16_t>(i);
		write_info.pixels = pixels;
		write_info.set_shader_visible = true;
		WriteTexture(a_temp_arena, write_info);
	}

	asset.hash = path_hash;
	asset.path = PathString();

	asset.image->width = uwidth;
	asset.image->height = uheight;
	asset.image->array_layers = array_layers;
	asset.image->gpu_image = gpu_image;
	asset.image->descriptor_index = descriptor_index;
	asset.image->asset_handle = AssetHandle(asset.hash.full_hash);

	const PathString icon_path = GetIconPathFromAssetName(asset.name.GetView());
	if (OSFileExist(icon_path.c_str()))
	{
		asset.icon_index = LoadIconFromPath(a_temp_arena, icon_path.GetView(), true);
	}
	else
	{
		const void* icon_write = pixels;
		if (uwidth != ICON_EXTENT.x || uheight != ICON_EXTENT.y)
		{
			icon_write = ResizeImage(a_temp_arena, pixels, width, height, static_cast<int>(ICON_EXTENT.x), static_cast<int>(ICON_EXTENT.y));
		}
		asset.icon_index = LoadIconFromPixels(a_temp_arena, icon_write, true);
		WriteImage(icon_path.GetView(), uint2(ICON_EXTENT.x, ICON_EXTENT.y), 4, icon_write);
	}

	STBI_FREE(pixels);
	asset.finished_loading = true;
	return *asset.image;
}

bool Asset::ReadWriteTextureDeferred(const StringView& a_path, const ImageInfo& a_image_info)
{
	const uint32_t readback_size = a_image_info.extent.x * a_image_info.extent.y * 4u;

	GPUBufferCreateInfo readback_info;
	readback_info.name = "viewport screenshot readback";
	readback_info.size = static_cast<uint64_t>(readback_size);
	readback_info.type = BUFFER_TYPE::READBACK;
	readback_info.host_writable = true;
	GPUBuffer readback = CreateGPUBuffer(readback_info);

	ReadImageInfo read_info;
	read_info.image_info = a_image_info;
	read_info.readback_size = readback_size;
	read_info.readback = readback;
	const GPUFenceValue fence_value = ReadTexture(read_info);

	WriteToDisk_params params;
	params.readback = readback;
	params.image_extent = a_image_info.extent;
	params.write_path = a_path;
	params.write_path.AddPathNoSlash(".png");

	return AddGPUTask(WriteToDisk_impl, params, fence_value);
}

bool Asset::WriteImage(const StringView& a_path, const uint2 a_extent, const uint32_t a_channels, const void* a_pixels)
{
	if (stbi_write_png(a_path.c_str(), static_cast<int>(a_extent.x), static_cast<int>(a_extent.y), static_cast<int>(a_channels), a_pixels, static_cast<int>(a_extent.x * a_channels)))
		return true;

	if (const char* stbi_fail_msg = stbi_failure_reason())
		BB_WARNING(false, stbi_fail_msg, WarningType::HIGH);
	return false;
}

unsigned char* Asset::LoadImageCPU(const StringView& a_path, int& a_width, int& a_height, int& a_bytes_per_pixel)
{
	stbi_uc* pixels = stbi_load(a_path.c_str(), &a_width, &a_height, &a_bytes_per_pixel, 4);
	a_bytes_per_pixel = 4;
	return pixels;
}

void Asset::FreeImageCPU(void* a_pixels)
{
	STBI_FREE(a_pixels);
}

const Image& Asset::LoadImageMemory(MemoryArena& a_temp_arena, const TextureLoadFromMemory& a_info)
{
	const AssetHash path_hash = CreateAssetHash(TurboCrappyImageHash(a_info.pixels, static_cast<size_t>(a_info.width) + a_info.height + a_info.bytes_per_pixel), ASSET_TYPE::IMAGE);
	bool exists = false;
	AssetSlot& asset = FindElementOrCreateElement(path_hash, exists);
	if (exists)
		return *asset.image;

	asset.name = a_info.name;

	IMAGE_FORMAT format;
	switch (a_info.bytes_per_pixel)
	{
	case 8:
		format = IMAGE_FORMAT::RGBA16_UNORM;
		break;
	case 4:
		format = IMAGE_FORMAT::RGBA8_SRGB;
		break;
	case 3:
		format = IMAGE_FORMAT::RGB8_SRGB;
		break;
	case 1:
		format = IMAGE_FORMAT::A8_UNORM;
		break;
	default:
		BB_ASSERT(false, "Current unsupported image bitcount.");
		format = IMAGE_FORMAT::RGBA8_SRGB;
		break;
	}

	RImage gpu_image;
	RDescriptorIndex descriptor_index;
	CreateImage_func(asset.name.GetView(), a_info.width, a_info.height, 1, format, IMAGE_VIEW_TYPE::TYPE_2D, gpu_image, descriptor_index);

	WriteImageInfo write_info{};
	write_info.image_info.image = gpu_image;
	write_info.image_info.extent = { a_info.width, a_info.height };
	write_info.image_info.mip_level = 0;
	write_info.image_info.array_layers = 1;
	write_info.image_info.base_array_layer = 0;
	write_info.format = format;
	write_info.pixels = a_info.pixels;
	write_info.set_shader_visible = true;
	WriteTexture(a_temp_arena, write_info);

	asset.hash = path_hash;
	asset.path = PathString();

	asset.image->width = a_info.width;
	asset.image->height = a_info.height;
	asset.image->array_layers = 1;
	asset.image->gpu_image = gpu_image;
	asset.image->descriptor_index = descriptor_index;
	asset.image->asset_handle = AssetHandle(path_hash.full_hash);

	const PathString icon_path = GetIconPathFromAssetName(asset.name.GetView());
	if (OSFileExist(icon_path.c_str()))
	{
		asset.icon_index = LoadIconFromPath(a_temp_arena, icon_path.GetView(), true);
	}
	else
	{
		const void* icon_write = a_info.pixels;
		if (a_info.width != ICON_EXTENT.x || a_info.height != ICON_EXTENT.y)
		{
			icon_write = ResizeImage(a_temp_arena, a_info.pixels, static_cast<int>(a_info.width), static_cast<int>(a_info.height), static_cast<int>(ICON_EXTENT.x), static_cast<int>(ICON_EXTENT.y));
		}
		asset.icon_index = LoadIconFromPixels(a_temp_arena, icon_write, true);
		const bool success = Asset::WriteImage(icon_path.GetView(), uint2(ICON_EXTENT.x, ICON_EXTENT.y), 4, icon_write);
		BB_WARNING(success, "failed to write icon to disk", WarningType::MEDIUM);
	}

	asset.finished_loading = true;
	return *asset.image;
}

static inline size_t CgltfGetMeshIndex(const cgltf_data& a_cgltf_data, const cgltf_mesh* a_mesh)
{
	return (reinterpret_cast<size_t>(a_mesh) - reinterpret_cast<size_t>(a_cgltf_data.meshes)) / sizeof(cgltf_mesh);
}

static inline size_t CgltfGetNodeIndex(const cgltf_data& a_cgltf_data, const cgltf_node* a_node)
{
	return (reinterpret_cast<size_t>(a_node) - reinterpret_cast<size_t>(a_cgltf_data.nodes)) / sizeof(cgltf_node);
}

static void LoadglTFNode(const cgltf_data& a_cgltf_data, Model& a_model, const size_t a_gltf_node_index)
{
	const cgltf_node& cgltf_node = a_cgltf_data.nodes[a_gltf_node_index];
	Model::Node& node = a_model.linear_nodes[a_gltf_node_index];
	if (cgltf_node.has_matrix)
	{
		float4x4 matrix;
		// maybe do conversion on row or not
		memcpy(&matrix, cgltf_node.matrix, sizeof(float4x4));
        matrix = Float4x4Transpose(matrix);
		Float4x4DecomposeTransform(matrix, node.translation, node.rotation, node.scale);
	}
	else
	{
		if (cgltf_node.has_translation)
			memcpy(&node.translation, cgltf_node.translation, sizeof(cgltf_node.translation));
		else
			node.translation = {};
		if (cgltf_node.has_rotation)
		{
			Quat rot;
			memcpy(&rot, cgltf_node.rotation, sizeof(Quat));

			node.rotation = Float3x3FromQuat(rot);
		}
		else
			node.rotation = Float3x3Identity();
		if (cgltf_node.has_scale)
			node.scale = float3(cgltf_node.scale[0], cgltf_node.scale[1], cgltf_node.scale[2]);
		else
			node.scale = float3(1.f, 1.f, 1.f);
	}

	if (cgltf_node.name)
		node.name = StringView(cgltf_node.name);
	else
		node.name = StringView("unnamed", _countof("unnamed"));
	
	if (cgltf_node.mesh != nullptr)
		node.mesh = &a_model.meshes[CgltfGetMeshIndex(a_cgltf_data, cgltf_node.mesh)];
	else
		node.mesh = nullptr;

	node.child_count = static_cast<uint32_t>(cgltf_node.children_count);
	if (node.child_count != 0)
	{
		const size_t cgltf_base_child_index = CgltfGetNodeIndex(a_cgltf_data, cgltf_node.children[0]);
		node.childeren = &a_model.linear_nodes[cgltf_base_child_index];
		for (size_t i = 0; i < node.child_count; i++)
		{
			const size_t child_index = CgltfGetNodeIndex(a_cgltf_data, cgltf_node.children[i]);
			BB_ASSERT(cgltf_base_child_index + i == child_index, "childeren are not sequentually loaded! Create a new solution");
			LoadglTFNode(a_cgltf_data, a_model, child_index);
		}
	}
}

static inline bool GenerateTangents(Slice<float3> a_tangents, const ConstSlice<float3> a_positions, const ConstSlice<float3> a_normals, const ConstSlice<float2> a_uvs, const ConstSlice<uint32_t> a_indices)
{
#if 1 // MIKKT
	if (a_positions.size() == 0 && a_normals.size() && a_uvs.size())
		return false;

	struct MikktUserData
	{
		Slice<float3> tangents;
		const ConstSlice<float3> positions;
		const ConstSlice<float3> normals;
		const ConstSlice<float2> uvs;
		const ConstSlice<uint32_t> indices;
	};

	SMikkTSpaceInterface mikkt_interface = {};
	mikkt_interface.m_getNumFaces = [](const SMikkTSpaceContext* pContext) -> int
		{
			MikktUserData* user_data = reinterpret_cast<MikktUserData*>(pContext->m_pUserData);
			return static_cast<int>(user_data->indices.size() / 3);
		};

	mikkt_interface.m_getNumVerticesOfFace = [](const SMikkTSpaceContext*, const int) -> int
		{
			return 3;
		};

	mikkt_interface.m_getPosition = [](const SMikkTSpaceContext* pContext, float fvPosOut[], const int iFace, const int iVert)
		{
			MikktUserData* user_data = reinterpret_cast<MikktUserData*>(pContext->m_pUserData);
			const size_t index = user_data->indices[static_cast<size_t>(iFace * pContext->m_pInterface->m_getNumVerticesOfFace(pContext, iFace) + iVert)];
			const float3& pos = user_data->positions[index];
			fvPosOut[0] = pos.x;
			fvPosOut[1] = pos.y;
			fvPosOut[2] = pos.z;
		};

	mikkt_interface.m_getNormal = [](const SMikkTSpaceContext* pContext, float fvNormOut[], const int iFace, const int iVert)
		{
			MikktUserData* user_data = reinterpret_cast<MikktUserData*>(pContext->m_pUserData);
			const size_t index = user_data->indices[static_cast<size_t>(iFace * pContext->m_pInterface->m_getNumVerticesOfFace(pContext, iFace) + iVert)];
			const float3& normal = user_data->normals[index];
			fvNormOut[0] = normal.x;
			fvNormOut[1] = normal.y;
			fvNormOut[2] = normal.z;
		};

	mikkt_interface.m_getTexCoord = [](const SMikkTSpaceContext* pContext, float fvTexcOut[], const int iFace, const int iVert)
		{
			MikktUserData* user_data = reinterpret_cast<MikktUserData*>(pContext->m_pUserData);
			const size_t index = user_data->indices[static_cast<size_t>(iFace * pContext->m_pInterface->m_getNumVerticesOfFace(pContext, iFace) + iVert)];
			const float2& uvs = user_data->uvs[index];
			fvTexcOut[0] = uvs.x;
			fvTexcOut[1] = uvs.y;
		};

	mikkt_interface.m_setTSpaceBasic = [](const SMikkTSpaceContext * pContext, const float fvTangent[], const float fSign, const int iFace, const int iVert)
		{
			MikktUserData* user_data = reinterpret_cast<MikktUserData*>(pContext->m_pUserData);
			const size_t index = user_data->indices[static_cast<size_t>(iFace * pContext->m_pInterface->m_getNumVerticesOfFace(pContext, iFace) + iVert)];
			float3& tangent = user_data->tangents[index];
			(void)fSign;
			tangent.x = fvTangent[0];
			tangent.y = fvTangent[1];
			tangent.z = fvTangent[2];
			//tangent.w = fSign;
		};

	MikktUserData user_data{ a_tangents, a_positions, a_normals, a_uvs, a_indices };

	SMikkTSpaceContext mikkt_context = {};
	mikkt_context.m_pInterface = &mikkt_interface;
	mikkt_context.m_pUserData = &user_data;

	return genTangSpaceDefault(&mikkt_context);

#else // custom
	// Jan's bootleg tangets that is quite fast
	for (size_t i = 0; i < a_indices.size(); i += 3)
	{
		Vertex& v0 = a_vertices[a_indices[i]];
		Vertex& v1 = a_vertices[a_indices[i + 1]];
		Vertex& v2 = a_vertices[a_indices[i + 2]];

		const float3 tangent0 = Float3Cross(v0.normal, float3(0.0f, -1.0f, 0.1f));
		const float3 tangent1 = Float3Cross(v1.normal, float3(0.0f, -1.0f, 0.1f));
		const float3 tangent2 = Float3Cross(v2.normal, float3(0.0f, -1.0f, 0.1f));

		v0.tangent = float4(tangent0, 0.f);
		v1.tangent = float4(tangent1, 0.f);
		v2.tangent = float4(tangent2, 0.f);
	}
	return true;
#endif 
}

static inline void* GetAccessorDataPtr(const cgltf_accessor* a_accessor)
{
	const size_t accessor_offset = a_accessor->buffer_view->offset + a_accessor->offset;
	return Pointer::Add(a_accessor->buffer_view->buffer->data, accessor_offset);
}

static inline BoundingBox GetBoundingBoxPrimitive(const ConstSlice<float3> a_vertices, const ConstSlice<uint32_t> a_indices, const uint32_t a_index_start, const uint32_t a_index_count)
{
    BoundingBox box;
    box.min = a_vertices[a_index_start];
    box.max = a_vertices[a_index_start];

    for (uint32_t i = a_index_start; i < a_index_count + a_index_start; i++)
    {
        const uint32_t index = a_indices[i];
        const float3 vert = a_vertices[index];
        box.min.x = Min(vert.x, box.min.x);
        box.min.y = Min(vert.y, box.min.y);
        box.min.z = Min(vert.z, box.min.z);

        box.max.x = Max(vert.x, box.max.x);
        box.max.y = Max(vert.y, box.max.y);
        box.max.z = Max(vert.z, box.max.z);
    }

    return box;
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

	CreateMeshInfo create_mesh{};

	uint32_t index_offset = 0;
	uint32_t vertex_pos_offset = 0;
	uint32_t vertex_normal_offset = 0;
	uint32_t vertex_uv_offset = 0;
	uint32_t vertex_color_offset = 0;
	uint32_t vertex_tangent_offset = 0;

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

		model_prim.material_data.material = Material::GetDefaultMasterMaterial(PASS_TYPE::SCENE, MATERIAL_TYPE::MATERIAL_3D);

		if (prim.material->pbr_metallic_roughness.base_color_texture.texture)
		{
			const cgltf_image& image = *prim.material->pbr_metallic_roughness.base_color_texture.texture->image;
			metallic_info.albedo_texture = Asset::LoadImageDisk(a_temp_arena, image.uri, IMAGE_FORMAT::RGBA8_SRGB).descriptor_index;
		}
		else
			metallic_info.albedo_texture = Asset::GetWhiteTexture();

		if (prim.material->normal_texture.texture)
		{
			const cgltf_image& image = *prim.material->normal_texture.texture->image;
			metallic_info.normal_texture = Asset::LoadImageDisk(a_temp_arena, image.uri, IMAGE_FORMAT::RGBA8_UNORM).descriptor_index;
		}
		else
			metallic_info.normal_texture = Asset::GetWhiteTexture();


		bool texture_is_orm = false;
		if (prim.material->has_pbr_metallic_roughness)
		{
			if (prim.material->occlusion_texture.texture == nullptr)
			{
				if (prim.material->pbr_metallic_roughness.metallic_roughness_texture.texture == nullptr)
					texture_is_orm = false;
				else
					texture_is_orm = true;
			}
			else
				texture_is_orm = prim.material->occlusion_texture.texture == prim.material->pbr_metallic_roughness.metallic_roughness_texture.texture;
		}

		if (texture_is_orm)
		{
			const cgltf_image& image = *prim.material->pbr_metallic_roughness.metallic_roughness_texture.texture->image;
			metallic_info.orm_texture = Asset::LoadImageDisk(a_temp_arena, image.uri, IMAGE_FORMAT::RGBA8_UNORM).descriptor_index;
		}
		else
		{
			metallic_info.orm_texture = Asset::GetWhiteTexture();
		}


		{	// get indices
			void* index_data = GetAccessorDataPtr(prim.indices);
			uint32_t* indices = reinterpret_cast<uint32_t*>(index_data);
			if (prim.indices->component_type == cgltf_component_type_r_16u)
			{
				indices = ArenaAllocArr(a_temp_arena, uint32_t, prim.indices->count);
				const uint16_t* index_data16 = reinterpret_cast<const uint16_t*>(index_data);
				for (size_t i = 0; i < prim.indices->count; i++)
					indices[index_offset + i] = index_data16[i];
				index_offset += static_cast<uint32_t>(prim.indices->count);
			}
			else
				BB_ASSERT(prim.indices->component_type == cgltf_component_type_r_32u, "GLTF mesh has an index type that is not supported!");

			create_mesh.indices = ConstSlice<uint32_t>(indices, prim.indices->count);
		}

		for (size_t attrib_index = 0; attrib_index < prim.attributes_count; attrib_index++)
		{
			const cgltf_attribute& attrib = prim.attributes[attrib_index];

			size_t element_size;
			switch (attrib.data->type)
			{
			case cgltf_type_scalar:
				element_size = sizeof(float);
				break;
			case cgltf_type_vec2:
				element_size = sizeof(float2);
				break;
			case cgltf_type_vec3:
				element_size = sizeof(float3);
				break;
			case cgltf_type_vec4:
				element_size = sizeof(float4);
				break;
			default:
				element_size = 0;
				BB_ASSERT(false, "invalid cgltf_type, parsing is messed up");
				break;
			}
			const size_t byte_stride = attrib.data->stride == 0 ? element_size : attrib.data->stride;
			const bool is_interleaved = element_size != byte_stride;

			float* data_pos = reinterpret_cast<float*>(GetAccessorDataPtr(attrib.data));
			switch (attrib.type)
			{
			case cgltf_attribute_type_position:
			{
                BB_ASSERT(vertex_pos_offset == 0, "already got position");
				float3* positions;
				BB_ASSERT(attrib.data->type == cgltf_type_vec3, "position is not vec3");
				if (is_interleaved)
				{
					positions = ArenaAllocArr(a_temp_arena, float3, vertex_count);
					for (size_t i = 0; i < attrib.data->count; i++)
					{
						positions[vertex_pos_offset].x = data_pos[0];
						positions[vertex_pos_offset].y = data_pos[1];
						positions[vertex_pos_offset].z = data_pos[2];

						data_pos = reinterpret_cast<float*>(Pointer::Add(data_pos, attrib.data->stride));
						++vertex_pos_offset;
					}
				}
				else
				{
					BB_ASSERT(attrib.data->count == vertex_count, "vertex count is not equal while data is not interleaved");
					positions = reinterpret_cast<float3*>(data_pos);
				}
				create_mesh.positions = ConstSlice<float3>(positions, attrib.data->count);
			}
				break;
			case cgltf_attribute_type_normal:
			{
                BB_ASSERT(vertex_normal_offset == 0, "already got normals");
				float3* normals;
				BB_ASSERT(attrib.data->type == cgltf_type_vec3, "normal is not vec3");
				if (is_interleaved)
				{
					normals = ArenaAllocArr(a_temp_arena, float3, vertex_count);
					for (size_t i = 0; i < attrib.data->count; i++)
					{
						normals[vertex_normal_offset].x = data_pos[0];
						normals[vertex_normal_offset].y = data_pos[1];
						normals[vertex_normal_offset].z = data_pos[2];

						data_pos = reinterpret_cast<float*>(Pointer::Add(data_pos, attrib.data->stride));
						++vertex_normal_offset;
					}
				}
				else
				{
					BB_ASSERT(attrib.data->count == vertex_count, "vertex count is not equal while data is not interleaved");
					normals = reinterpret_cast<float3*>(data_pos);
				}
				create_mesh.normals = ConstSlice<float3>(normals, attrib.data->count);
			}
				break;
			case cgltf_attribute_type_texcoord:
			{
                BB_ASSERT(vertex_uv_offset == 0, "already got uvs");
				float2* uvs;
				BB_ASSERT(attrib.data->type == cgltf_type_vec2, "uv is not vec3");
				if (is_interleaved)
				{
					uvs = ArenaAllocArr(a_temp_arena, float2, vertex_count);
					for (size_t i = 0; i < attrib.data->count; i++)
					{
						uvs[vertex_uv_offset].x = data_pos[0];
						uvs[vertex_uv_offset].y = data_pos[1];

						data_pos = reinterpret_cast<float*>(Pointer::Add(data_pos, attrib.data->stride));
						++vertex_uv_offset;
					}
				}
				else
				{
					BB_ASSERT(attrib.data->count == vertex_count, "vertex count is not equal while data is not interleaved");
					uvs = reinterpret_cast<float2*>(data_pos);
				}
				create_mesh.uvs = ConstSlice<float2>(uvs, attrib.data->count);
			}
				break;
			case cgltf_attribute_type_color:
			{
                BB_ASSERT(vertex_color_offset == 0, "already got colors");
				float4* colors;
				BB_ASSERT(attrib.data->type == cgltf_type_vec4, "color is not vec4");
				if (is_interleaved)
				{
					colors = ArenaAllocArr(a_temp_arena, float4, vertex_count);
					for (size_t i = 0; i < attrib.data->count; i++)
					{
						colors[vertex_color_offset].x = data_pos[0];
						colors[vertex_color_offset].y = data_pos[1];
						colors[vertex_color_offset].z = data_pos[2];
						colors[vertex_color_offset].w = data_pos[3];

						data_pos = reinterpret_cast<float*>(Pointer::Add(data_pos, attrib.data->stride));
						++vertex_color_offset;
					}
				}
				else
				{
					BB_ASSERT(attrib.data->count == vertex_count, "vertex count is not equal while data is not interleaved");
					colors = reinterpret_cast<float4*>(data_pos);
				}
				create_mesh.colors = ConstSlice<float4>(colors, attrib.data->count);
			}
				break;
			case cgltf_attribute_type_tangent:
			{
                BB_ASSERT(vertex_tangent_offset == 0, "already got tangents");
				float3* tangents;
				BB_ASSERT(attrib.data->type == cgltf_type_vec3, "tangents is not vec3");
				if (is_interleaved)
				{
					tangents = ArenaAllocArr(a_temp_arena, float3, vertex_count);
					for (size_t i = 0; i < attrib.data->count; i++)
					{
						tangents[vertex_tangent_offset].x = data_pos[0];
						tangents[vertex_tangent_offset].y = data_pos[1];
						tangents[vertex_tangent_offset].z = data_pos[2];

						data_pos = reinterpret_cast<float*>(Pointer::Add(data_pos, attrib.data->stride));
						++vertex_tangent_offset;
					}
				}
				else
				{
					BB_ASSERT(attrib.data->count == vertex_count, "vertex count is not equal while data is not interleaved");
					tangents = reinterpret_cast<float3*>(data_pos);
				}
				create_mesh.tangents = ConstSlice<float3>(tangents, attrib.data->count);
			}
				break;
			default:
				break;
			}
		}
		BB_ASSERT(index_count + 1 > index_offset, "overwriting gltf Index Memory!");
		BB_ASSERT(vertex_count + 1 > vertex_pos_offset, "overwriting gltf Vertex Memory!");
		BB_ASSERT(vertex_count + 1 > vertex_normal_offset, "overwriting gltf Vertex Memory!");
		BB_ASSERT(vertex_count + 1 > vertex_uv_offset, "overwriting gltf Vertex Memory!");
		BB_ASSERT(vertex_count + 1 > vertex_color_offset, "overwriting gltf Vertex Memory!");
		BB_ASSERT(vertex_count + 1 > vertex_tangent_offset, "overwriting gltf Vertex Memory!");

		// tangents not calculated, do it yourself
		if (vertex_tangent_offset == 0)
		{
			float3* tangents = ArenaAllocArr(a_temp_arena, float3, vertex_count);
			GenerateTangents(Slice(tangents, vertex_count), create_mesh.positions, create_mesh.normals, create_mesh.uvs, create_mesh.indices);
			create_mesh.tangents = ConstSlice<float3>(tangents, vertex_count);
		}

		if (vertex_color_offset == 0)
		{
			float4* colors = ArenaAllocArr(a_temp_arena, float4, vertex_count);
			for (size_t i = 0; i < vertex_count; i++)
				colors[i] = float4(1.f);
			create_mesh.colors = ConstSlice<float4>(colors, vertex_count);
		}
	}
    for (size_t prim_index = 0; prim_index < mesh.primitives_count; prim_index++)
    {
        Model::Primitive& model_prim = a_mesh.primitives[prim_index];
        model_prim.bounding_box = GetBoundingBoxPrimitive(create_mesh.positions, create_mesh.indices, model_prim.start_index, model_prim.index_count);
    }
	CreateMesh(a_temp_arena, create_mesh, a_mesh.mesh);
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

const Model& Asset::LoadglTFModel(MemoryArena& a_temp_arena, const MeshLoadFromDisk& a_mesh_op)
{
	const AssetHash asset_hash = CreateAssetHash(StringHash(a_mesh_op.path), ASSET_TYPE::MODEL);
	bool exists = false;
	AssetSlot& asset = FindElementOrCreateElement(asset_hash, exists);
	if (exists)
		return *asset.model;

	cgltf_options gltf_option = {};
	gltf_option.memory.alloc_func = cgltf_arena_alloc;
	gltf_option.memory.free_func = cgltf_arena_free;
	gltf_option.memory.user_data = &a_temp_arena;
	cgltf_data* gltf_data = nullptr;

	const PathString path = CreateModelPath(a_mesh_op.path);

	if (cgltf_parse_file(&gltf_option, path.c_str(), &gltf_data) != cgltf_result_success)
	{
		BB_ASSERT(false, "Failed to load glTF model, cgltf_parse_file.");
		return *asset.model;
	}

	cgltf_load_buffers(&gltf_option, gltf_data, path.c_str());

	if (cgltf_validate(gltf_data) != cgltf_result_success)
	{
		BB_ASSERT(false, "GLTF model validation failed!");
		return *asset.model;
	}
	const uint32_t linear_node_count = static_cast<uint32_t>(gltf_data->nodes_count);

	// JANK :( VERY UNHAPPY
	OSAcquireSRWLockWrite(&s_asset_manager->asset_lock);
	asset.model->meshes.Init(AssetAllocArr<Model::Mesh>(gltf_data->meshes_count), static_cast<uint32_t>(gltf_data->meshes_count));
	asset.model->meshes.resize(asset.model->meshes.capacity());
	for (size_t mesh_index = 0; mesh_index < asset.model->meshes.size(); mesh_index++)
	{
		const cgltf_mesh& cgltf_mesh = gltf_data->meshes[mesh_index];
		Model::Mesh& mesh = asset.model->meshes[mesh_index];

		mesh.primitives.Init(AssetAllocArr<Model::Primitive>(cgltf_mesh.primitives_count), static_cast<uint32_t>(cgltf_mesh.primitives_count));
		mesh.primitives.resize(mesh.primitives.capacity());
	}

	asset.model->linear_nodes = AssetAllocArr<Model::Node>(linear_node_count);
	asset.model->root_node_count = static_cast<uint32_t>(gltf_data->scene->nodes_count);
	asset.model->root_node_indices = AssetAllocArr<uint32_t>(asset.model->root_node_count);
	OSReleaseSRWLockWrite(&s_asset_manager->asset_lock);

	// for every 5 meshes create a thread;
	constexpr size_t TASKS_PER_THREAD = 5;

	const uint32_t thread_count = asset.model->meshes.size() / TASKS_PER_THREAD;

	// +1 due to main thread also working.
	Threads::Barrier barrier(thread_count + 1);
	size_t task_index = 0;
	for (uint32_t i = 0; i < thread_count; i++)
	{
		LoadgltfMeshBatch_params batch;
		batch.barrier = &barrier;
		batch.cgltf_meshes = Slice(&gltf_data->meshes[task_index], TASKS_PER_THREAD);
		batch.meshes = Slice(&asset.model->meshes[task_index], TASKS_PER_THREAD);
		Threads::StartTaskThread(LoadgltfMeshBatch, &batch, sizeof(batch), L"gltf mesh upload batch");

		task_index += TASKS_PER_THREAD;
	}

	MemoryArenaScope(a_temp_arena)
	{
		for (; task_index < asset.model->meshes.size(); task_index++)
		{
			LoadglTFMesh(a_temp_arena, gltf_data->meshes[task_index], asset.model->meshes[task_index]);
		}
	}
	barrier.Signal();
	// check if we have done all the work required.
	barrier.Wait();

	for (size_t i = 0; i < gltf_data->scene->nodes_count; i++)
	{
		const size_t gltf_node_index = CgltfGetNodeIndex(*gltf_data, gltf_data->scene->nodes[i]);
		LoadglTFNode(*gltf_data, *asset.model, gltf_node_index);
		asset.model->root_node_indices[i] = static_cast<uint32_t>(gltf_node_index);
	}
	cgltf_free(gltf_data);

	GetAssetNameFromPath(a_mesh_op.path, asset.name);
	
	asset.hash = asset_hash;
	asset.path = a_mesh_op.path;
	asset.model->asset_handle = AssetHandle(asset.hash.full_hash);

	const PathString icon_path = GetIconPathFromAssetName(asset.name.GetView());
	if (OSFileExist(icon_path.c_str()))
		asset.icon_index = LoadIconFromPath(a_temp_arena, icon_path.GetView(), true);
	else
		asset.icon_index = 0;

	asset.finished_loading = true;
	return *asset.model;
}

const Model& Asset::LoadMeshFromMemory(MemoryArena& a_temp_arena, const MeshLoadFromMemory& a_mesh_op)
{
	// this is all garbage

	const AssetHash asset_hash = CreateAssetHash(StringHash(a_mesh_op.name), ASSET_TYPE::MODEL);
	bool exists = false;
	AssetSlot& asset = FindElementOrCreateElement(asset_hash, exists);
	if (exists)
		return *asset.model;

	asset.hash = asset_hash;
	asset.path = PathString();
	asset.name = a_mesh_op.name;

	CreateMeshInfo create_mesh;
	create_mesh.indices = a_mesh_op.indices;
	create_mesh.positions = a_mesh_op.mesh_load.positions;
	create_mesh.normals = a_mesh_op.mesh_load.normals;
	create_mesh.uvs = a_mesh_op.mesh_load.uvs;
	create_mesh.colors = a_mesh_op.mesh_load.colors;

	float3* tangents = ArenaAllocArr(a_temp_arena, float3, a_mesh_op.mesh_load.positions.size());
	GenerateTangents(Slice(tangents, a_mesh_op.mesh_load.positions.size()), create_mesh.positions, create_mesh.normals, create_mesh.uvs, create_mesh.indices);
	create_mesh.tangents = ConstSlice<float3>(tangents, a_mesh_op.mesh_load.positions.size());

	OSAcquireSRWLockWrite(&s_asset_manager->asset_lock);
	//hack shit way, but a single mesh just has one primitive to draw.
	asset.model->linear_nodes = AssetAlloc<Model::Node>();
	asset.model->meshes.Init(AssetAlloc<Model::Mesh>(), 1, 1);
	asset.model->meshes[0].primitives.Init(AssetAlloc<Model::Primitive>(), 1, 1);
	asset.model->root_node_indices = AssetAlloc<uint32_t>();
	OSReleaseSRWLockWrite(&s_asset_manager->asset_lock);
	Model::Primitive primitive;
	primitive.material_data.mesh_metallic.albedo_texture = a_mesh_op.base_albedo;
	primitive.material_data.mesh_metallic.normal_texture = GetBlueTexture();
	primitive.start_index = 0;
	primitive.index_count = static_cast<uint32_t>(a_mesh_op.indices.size());

	Model::Mesh& mesh = asset.model->meshes[0];
	mesh.primitives[0] = primitive;
    mesh.primitives[0].bounding_box = GetBoundingBoxPrimitive(create_mesh.positions, create_mesh.indices, mesh.primitives[0].start_index, mesh.primitives[0].index_count);
	CreateMesh(a_temp_arena, create_mesh, mesh.mesh);

	*asset.model->root_node_indices = 0;

	asset.model->linear_nodes[0] = {};
	asset.model->linear_nodes[0].name = a_mesh_op.name;
	asset.model->linear_nodes[0].child_count = 0;
	asset.model->linear_nodes[0].mesh = &mesh;
	asset.model->linear_nodes[0].scale = float3(1.f, 1.f, 1.f);
	asset.model->asset_handle = AssetHandle(asset.hash.full_hash);

	const PathString icon_path = GetIconPathFromAssetName(asset.name.c_str());
	if (OSFileExist(icon_path.c_str()))
		asset.icon_index = LoadIconFromPath(a_temp_arena, icon_path.GetView(), true);
	else
		asset.icon_index = 0;

    asset.finished_loading = true;

	return *asset.model;
}

const Model* Asset::FindModelByName(const char* a_name)
{
	AssetHash asset_hash = CreateAssetHash(StringHash(a_name), ASSET_TYPE::MODEL);
	if (AssetSlot* slot = s_asset_manager->asset_table.find(asset_hash.full_hash))
	{
		while (!slot->finished_loading) {}
		return slot->model;
	}
	return nullptr;
}

const Image* Asset::FindImageByName(const char* a_name)
{
	AssetHash asset_hash = CreateAssetHash(StringHash(a_name), ASSET_TYPE::IMAGE);
	if (AssetSlot* slot = s_asset_manager->asset_table.find(asset_hash.full_hash))
	{
		while (!slot->finished_loading) {}
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

		for (size_t i = 0; i < slot->model->meshes.size(); i++)
			AssetFree(slot->model->meshes[i].primitives.data());
		AssetFree(slot->model->linear_nodes);
		AssetFree(slot->model->meshes.data());
		AssetFree(slot->model);
		break;
	case ASSET_TYPE::IMAGE:
		AssetFree(slot->image);
		break;
	default:
		BB_ASSERT(false, "default hit while it shouldn't");
		break;
	}

	OSAcquireSRWLockWrite(&s_asset_manager->asset_lock);
	// TODO, erase from the linear array as well
	s_asset_manager->asset_table.erase(a_asset_handle.handle);
	OSReleaseSRWLockWrite(&s_asset_manager->asset_lock);
}

#include "imgui.h"

constexpr size_t ASSET_SEARCH_PATH_SIZE_MAX = 512;

static void LoadAssetViaSearch(MemoryArena& a_temp_arena)
{
	static StackString<ASSET_SEARCH_PATH_SIZE_MAX> search_path;

	if (OSFindFileNameDialogWindow(search_path.data(), search_path.capacity()))
	{
		search_path.RecalculateStringSize();
		const size_t get_extension_pos = search_path.find_last_of('.');
		const size_t get_file_name = search_path.find_last_of('\\');
		BB_ASSERT(get_file_name != size_t(-1), "i fucked up");

		Asset::AsyncAsset asset{};

		// we always load from disk due to the nature of loading an asset via a path
		asset.load_type = Asset::ASYNC_LOAD_TYPE::DISK;

		if (search_path.compare(get_extension_pos, ".gltf"))
		{
			asset.asset_type = Asset::ASYNC_ASSET_TYPE::MODEL;
			asset.mesh_disk.path = search_path.GetView();
		}
		else if (search_path.compare(get_extension_pos, ".png") || search_path.compare(get_extension_pos, ".jpg") || search_path.compare(get_extension_pos, ".bmp"))
		{
			asset.asset_type = Asset::ASYNC_ASSET_TYPE::TEXTURE;
			asset.texture_disk.path = search_path.GetView();
		}
		else
		{
			BB_ASSERT(false, "NOT SUPPORTED FILE NAME!");
		}

		LoadAssetsASync(a_temp_arena, Slice(&asset, 1));
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
					LoadAssetViaSearch(a_arena);
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

				const char* asset_name = slot->name.c_str();
				if (!asset_name)
				{
					asset_name = "unnamed";
				}


				if (ImGui::CollapsingHeader(asset_name))
				{
					if (slot->path.size())
						ImGui::Text("Path: %s", slot->path.c_str());
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
								slot->icon_index = LoadIconFromPath(a_arena, path_search.GetView(), false);
								IconWriteToDisk(slot->icon_index, GetIconPathFromAssetName(slot->name.c_str()));
							}
						}
					}

					// show icon
					const float icons_texture_width = static_cast<float>(ICON_EXTENT.x * s_asset_manager->icons_storage.max_slots);
					const float slot_size_in_float = static_cast<float>(ICON_EXTENT.x) / icons_texture_width;

					const ImVec2 uv0(slot_size_in_float * static_cast<float>(slot->icon_index), 0);
					const ImVec2 uv1(uv0.x + slot_size_in_float, 1);

					ImGui::Image(s_asset_manager->icons_storage.image_descriptor_index.handle, ImVec2(ICON_EXTENT_F.x, ICON_EXTENT_F.y), uv0, uv1);
				}


				ImGui::PopID();
			}
			ImGui::EndTable();
		}
	}
	ImGui::End();
}

RDescriptorIndex Asset::GetWhiteTexture()
{
	return s_asset_manager->pre_loaded.white.index;
}

RDescriptorIndex Asset::GetBlackTexture()
{
	return s_asset_manager->pre_loaded.black.index;
}

RDescriptorIndex Asset::GetRedTexture()
{
	return s_asset_manager->pre_loaded.red.index;
}

RDescriptorIndex Asset::GetGreenTexture()
{
	return s_asset_manager->pre_loaded.green.index;
}

RDescriptorIndex Asset::GetBlueTexture()
{
	return s_asset_manager->pre_loaded.blue.index;
}

RDescriptorIndex Asset::GetCheckerBoardTexture()
{
	return s_asset_manager->pre_loaded.checkerboard.index;
}
