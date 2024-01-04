#include "AssetLoader.hpp"

#include "Storage/Hashmap.h"
#include "MemoryArena.hpp"
#include "BBIntrin.h"
#include "BBImage.hpp"
#include "Math.inl"
#include "Program.h"

#include "shared_common.hlsl.h"
#include "Renderer.hpp"



BB_WARNINGS_OFF
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
BB_WARNINGS_ON

using namespace BB;

//constexpr const char JSON_DIRECTORY[] = "../Resources/Json/";
//constexpr const char MODELS_DIRECTORY[] = "../Resources/Models/";
//constexpr const char SHADERS_DIRECTORY[] = "../Resources/Shaders/";
constexpr const char TEXTURE_DIRECTORY[] = "../Resources/Textures/";

static char* CreateGLTFImagePath(Allocator a_temp_allocator, const char* a_image_path)
{
	const size_t image_path_size = strlen(a_image_path);
	const size_t texture_directory_size = sizeof(TEXTURE_DIRECTORY);
	char* new_path = BBnewArr(a_temp_allocator, texture_directory_size + image_path_size, char);

	Memory::Copy(new_path, TEXTURE_DIRECTORY, sizeof(TEXTURE_DIRECTORY));
	Memory::Copy(&new_path[sizeof(TEXTURE_DIRECTORY) - 1], a_image_path, image_path_size);
	new_path[sizeof(TEXTURE_DIRECTORY) + image_path_size - 1] = '\0';

	return new_path;
}

//crappy hash, don't care for now.
static uint64_t StringHash(const char* a_string)
{
	uint64_t hash = 5381;
	uint32_t c;

	while ((c = *reinterpret_cast<const uint8_t*>(a_string++)))
		hash = ((hash << 5) + hash) + c;

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

	return b128_to_b64[0] + b128_to_b64[1];
}

enum class ASSET_TYPE : uint8_t
{
	MODEL,
	TEXTURE
};

//this hash works by setting a 64 bit hash, then overwriting the last byte to show which asset type it is.
union AssetHash
{
	uint64_t full_hash;
	struct
	{
		uint8_t hash[7];	// 7
		ASSET_TYPE type;	// 8
	};
};

static AssetHash CreateAssetHash(const uint64_t a_hash, const ASSET_TYPE a_type)
{
	AssetHash hash;
	hash.full_hash = a_hash;
	hash.type = a_type;
	return hash;
}

struct AssetSlot
{
	AssetHash hash;
	const char* path;
	union
	{
		Model* model;
		Image* image;
	};
};

struct AssetManager
{
	BBRWLock asset_lock;
	StaticOL_HashMap<uint64_t, AssetSlot> asset_table;
	BBRWLock string_lock;
	StaticOL_HashMap<uint64_t, char*> string_table;
	struct StringBuffer
	{
		char* current_memory_pos;
		size_t mem_remaining;
	} string_buffer;

	MemoryArena asset_arena;
};
static AssetManager* s_asset_manager;

static inline char* AllocateStringSpace(const size_t a_string_size)
{
	if (s_asset_manager->string_buffer.mem_remaining > a_string_size)
		s_asset_manager->string_buffer.mem_remaining -= a_string_size;
	else
		BB_ASSERT(false, "not enough string space in asset manager!");

	char* string_mem = s_asset_manager->string_buffer.current_memory_pos;
	s_asset_manager->string_buffer.current_memory_pos += a_string_size;
	return string_mem;
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
	s_asset_manager->string_table.Init(s_asset_manager->asset_arena, a_init_info.string_entry_count);

	s_asset_manager->string_buffer.current_memory_pos = ArenaAllocArr(s_asset_manager->asset_arena, char, a_init_info.string_memory_size);
	s_asset_manager->string_buffer.mem_remaining = a_init_info.string_memory_size;
	Memory::Set(s_asset_manager->string_buffer.current_memory_pos, 0, a_init_info.string_memory_size);
	//now store the memory arena
}

const char* Asset::FindOrCreateString(const char* a_string)
{
	const uint64_t string_hash = StringHash(a_string);
	char** string_ptr = s_asset_manager->string_table.find(string_hash);
	if (string_ptr != nullptr)
		return *string_ptr;

	const uint32_t string_size = static_cast<uint32_t>(strlen(a_string) + 1);

	char* const string = AllocateStringSpace(string_size);
	memcpy(string, a_string, string_size);
	string[string_size - 1] = '\0';

	OSAcquireSRWLockWrite(&s_asset_manager->string_lock);
	s_asset_manager->string_table.emplace(string_hash, string);
	OSReleaseSRWLockWrite(&s_asset_manager->string_lock);
	return string;
}

void Asset::LoadASync(const BB::Slice<AsyncAsset> a_asyn_assets, const char* a_task_name)
{
	LinearAllocator_t load_async_allocator = { 8 * mbSize, a_task_name };

	UploadBufferView& upload_buffer_view = GetUploadView(0);
	CommandPool& cmd_pool = GetTransferCommandPool();
	const RCommandList cmd_list = cmd_pool.StartCommandList();

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
				LoadglTFModel(load_async_allocator, task.mesh_disk, cmd_list, upload_buffer_view);
				break;
			case ASYNC_LOAD_TYPE::MEMORY:
				LoadMeshFromMemory(task.mesh_memory, cmd_list, upload_buffer_view);
				break;
			}
			break;
		}
		case ASYNC_ASSET_TYPE::TEXTURE:
		{
			switch (task.load_type)
			{
			case ASYNC_LOAD_TYPE::DISK:
				BB_ASSERT(false, "no disk load for textures yet.");
				break;
			case ASYNC_LOAD_TYPE::MEMORY:
				LoadImageMemory(*task.texture_memory.image, task.texture_memory.name, cmd_list, upload_buffer_view);
				break;
			}
		}
		break;
		}
	}
	cmd_pool.EndCommandList(cmd_list);
	BB_ASSERT(ExecuteTransferCommands(Slice(&cmd_pool, 1), Slice(&upload_buffer_view, 1)), "Failed to execute async gpu transfer commands");
	load_async_allocator.Clear();
}

const Image* Asset::LoadImageDisk(const char* a_path, const char* a_name, const RCommandList a_list, UploadBufferView& a_upload_view)
{
	int x = 0, y = 0, channels = 0;
	//hacky way, whatever we do it for now.
	stbi_uc* pixels = stbi_load(a_path, &x, &y, &channels, 4);

	if (!pixels)
	{
		BB_ASSERT(false, "failed to load image!");
		return nullptr;
	}

	UploadImageInfo upload_image_info;
	upload_image_info.name = a_name;
	upload_image_info.pixels = pixels;
	upload_image_info.width = static_cast<uint32_t>(x);
	upload_image_info.height = static_cast<uint32_t>(y);
	upload_image_info.format = IMAGE_FORMAT::RGBA8_SRGB;
	upload_image_info.usage = IMAGE_USAGE::TEXTURE;

	const RTexture gpu_image = UploadTexture(a_list, upload_image_info, a_upload_view);


	OSAcquireSRWLockWrite(&s_asset_manager->asset_lock);

	Image* image = ArenaAllocType(s_asset_manager->asset_arena, Image);
	image->width = upload_image_info.width;
	image->height = upload_image_info.height;
	image->gpu_image = gpu_image;

	const uint64_t hash = TurboCrappyImageHash(pixels, static_cast<size_t>(image->width) + image->height + static_cast<uint32_t>(channels));
	BB_ASSERT(hash != 0, "Image hashing failed");

	STBI_FREE(pixels);

	AssetSlot asset;
	asset.hash = CreateAssetHash(hash, ASSET_TYPE::TEXTURE);
	asset.path = nullptr; //memory loads have nullptr as path.
	asset.image = image;

	s_asset_manager->asset_table.insert(asset.hash.full_hash, asset);
	image->asset_handle = AssetHandle(asset.hash.full_hash);

	OSReleaseSRWLockWrite(&s_asset_manager->asset_lock);

	return image;
}

const Image* Asset::LoadImageMemory(const BB::BBImage& a_image, const char* a_name, const RCommandList a_list, UploadBufferView& a_upload_view)
{
	UploadImageInfo upload_image_info;
	upload_image_info.name = a_name;
	upload_image_info.pixels = a_image.GetPixels();
	upload_image_info.width = a_image.GetWidth();
	upload_image_info.height = a_image.GetWidth();
	upload_image_info.usage = IMAGE_USAGE::TEXTURE;
	switch (a_image.GetBitCount())
	{
	case 32:
		upload_image_info.format = IMAGE_FORMAT::RGBA8_SRGB;
		break;
	case 8:
		upload_image_info.format = IMAGE_FORMAT::A8_UNORM;
		break;
	default:
		BB_ASSERT(false, "Current unsupported image bitcount.");
		upload_image_info.format = IMAGE_FORMAT::RGBA8_SRGB;
		break;
	}


	const RTexture gpu_image = UploadTexture(a_list, upload_image_info, a_upload_view);

	OSAcquireSRWLockWrite(&s_asset_manager->asset_lock);
	Image* image = ArenaAllocType(s_asset_manager->asset_arena, Image);
	image->width = upload_image_info.width;
	image->height = upload_image_info.height;
	image->gpu_image = gpu_image;

	const uint64_t hash = TurboCrappyImageHash(a_image.GetPixels(), static_cast<size_t>(image->width) + image->height + (a_image.GetBitCount() / 8));
	BB_ASSERT(hash != 0, "Image hashing failed");

	AssetSlot asset;
	asset.hash = CreateAssetHash(hash, ASSET_TYPE::TEXTURE);
	asset.path = nullptr; //memory loads have nullptr has path.
	asset.image = image;

	s_asset_manager->asset_table.insert(asset.hash.full_hash, asset);
	image->asset_handle = AssetHandle(asset.hash.full_hash);
	OSReleaseSRWLockWrite(&s_asset_manager->asset_lock);

	return image;
}

static inline void* GetAccessorDataPtr(const cgltf_accessor* a_Accessor)
{
	const size_t accessor_offset = a_Accessor->buffer_view->offset + a_Accessor->offset;
	return Pointer::Add(a_Accessor->buffer_view->buffer->data, accessor_offset);
}

static void LoadglTFNode(Allocator a_temp_allocator, Slice<ShaderEffectHandle> a_shader_effects, const RCommandList a_list, UploadBufferView& a_upload_view, const cgltf_node& a_node, Model& a_model, uint32_t& a_node_index, uint32_t& a_primitive_index)
{
	Model::Node& mod_node = a_model.linear_nodes[a_node_index++];
	if (a_node.has_matrix)
		memcpy(&mod_node.transform, a_node.matrix, sizeof(float4x4));
	else
		mod_node.transform = Float4x4Identity();

	if (a_node.mesh != nullptr)
	{
		const cgltf_mesh& mesh = *a_node.mesh;

		mod_node.primitives = &a_model.primitives[a_primitive_index];
		mod_node.primitive_count = static_cast<uint32_t>(mesh.primitives_count);

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

		uint32_t* indices = BBnewArr(a_temp_allocator, index_count, uint32_t);
		Vertex* vertices = BBnewArr(a_temp_allocator, vertex_count, Vertex);
		uint32_t index_offset = 0;
		uint32_t vertex_pos_offset = 0;
		uint32_t vertex_normal_offset = 0;
		uint32_t vertex_uv_offset = 0;

		for (size_t prim_index = 0; prim_index < mesh.primitives_count; prim_index++)
		{
			const cgltf_primitive& prim = mesh.primitives[prim_index];
			Model::Primitive& model_prim = a_model.primitives[a_primitive_index++];
			model_prim.start_index = index_offset;
			model_prim.index_count = static_cast<uint32_t>(prim.indices->count);

			CreateMaterialInfo material_info{};
			material_info.shader_effects = a_shader_effects;
			if (prim.material->pbr_metallic_roughness.base_color_texture.texture)
			{
				const cgltf_image& image = *prim.material->pbr_metallic_roughness.base_color_texture.texture->image;

				const char* full_image_path = CreateGLTFImagePath(a_temp_allocator, image.uri);
				const Image* img = Asset::LoadImageDisk(full_image_path, image.name, a_list, a_upload_view);

				material_info.base_color = img->gpu_image;
			}

			if (prim.material->normal_texture.texture)
			{
				const cgltf_image& image = *prim.material->normal_texture.texture->image;

				const char* full_image_path = CreateGLTFImagePath(a_temp_allocator, image.uri);
				const Image* img = Asset::LoadImageDisk(full_image_path, image.name, a_list, a_upload_view);
	
				material_info.normal_texture = img->gpu_image;
			}

			model_prim.material = CreateMaterial(material_info);

			{	//get indices
				void* index_data = GetAccessorDataPtr(prim.indices);
				if (prim.indices->component_type == cgltf_component_type_r_32u)
				{
					Memory::Copy(&indices[index_offset], index_data, prim.indices->count);
					index_offset += static_cast<uint32_t>(prim.indices->count);
				}
				else if (prim.indices->component_type == cgltf_component_type_r_16u)
				{
					for (size_t i = 0; i < prim.indices->count; i++)
						indices[index_offset + i] = reinterpret_cast<uint16_t*>(index_data)[i];
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

						data_pos = reinterpret_cast<const float*>(Pointer::Add(data_pos, attrib.data->stride));
						++vertex_pos_offset;
					}
					break;
				case cgltf_attribute_type_normal:
					for (size_t i = 0; i < attrib.data->count; i++)
					{
						vertices[vertex_normal_offset].normal.x = data_pos[0];
						vertices[vertex_normal_offset].normal.y = data_pos[1];
						vertices[vertex_normal_offset].normal.z = data_pos[2];

						data_pos = reinterpret_cast<const float*>(Pointer::Add(data_pos, attrib.data->stride));
						++vertex_normal_offset;
					}
					break;
				case cgltf_attribute_type_texcoord:
					for (size_t i = 0; i < attrib.data->count; i++)
					{
						vertices[vertex_uv_offset].uv.x = data_pos[0];
						vertices[vertex_uv_offset].uv.y = data_pos[1];

						data_pos = reinterpret_cast<const float*>(Pointer::Add(data_pos, attrib.data->stride));
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

		mod_node.mesh_handle = CreateMesh(a_list, create_mesh, a_upload_view);
	}
	else
		mod_node.mesh_handle = MeshHandle(BB_INVALID_HANDLE_64);

	mod_node.child_count = static_cast<uint32_t>(a_node.children_count);
	if (mod_node.child_count != 0)
	{
		mod_node.childeren = &a_model.linear_nodes[a_node_index]; //childeren are loaded linearly, i'm quite sure.
		for (size_t i = 0; i < mod_node.child_count; i++)
		{
			LoadglTFNode(a_temp_allocator, a_shader_effects, a_list, a_upload_view, *a_node.children[i], a_model, a_node_index, a_primitive_index);
		}
	}
}

const Model* Asset::LoadglTFModel(Allocator a_temp_allocator, const MeshLoadFromDisk& a_mesh_op, const RCommandList a_list, UploadBufferView& a_upload_view)
{
	const AssetHash asset_hash = CreateAssetHash(StringHash(a_mesh_op.path), ASSET_TYPE::MODEL);
	
	if (AssetSlot* slot = s_asset_manager->asset_table.find(asset_hash.full_hash))
	{
		return slot->model;
	}

	cgltf_options gltf_option = {};
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

	uint32_t primitive_count = 0;
	for (size_t mesh_index = 0; mesh_index < gltf_data->meshes_count; mesh_index++)
	{
		const cgltf_mesh& mesh = gltf_data->meshes[mesh_index];
		primitive_count += static_cast<uint32_t>(mesh.primitives_count);
	}

	const uint32_t linear_node_count = static_cast<uint32_t>(gltf_data->nodes_count);
	OSAcquireSRWLockWrite(&s_asset_manager->asset_lock);
	//optimize the memory space with one allocation for the entire model
	Model* model = ArenaAllocType(s_asset_manager->asset_arena, Model);
	model->linear_nodes = ArenaAllocArr(s_asset_manager->asset_arena, Model::Node, linear_node_count);
	model->primitives = ArenaAllocArr(s_asset_manager->asset_arena, Model::Primitive, primitive_count);
	//for now this is going to be the root node, which is 0.
	//This is not accurate on all GLTF models
	model->primitive_count = primitive_count;
	model->root_node_count = static_cast<uint32_t>(gltf_data->scene->nodes_count);
	BB_ASSERT(model->root_node_count == 1, "not supporting more then 1 root node for gltf yet.");
	model->root_nodes = &model->linear_nodes[0];
	OSReleaseSRWLockWrite(&s_asset_manager->asset_lock);
	uint32_t current_node = 0;
	uint32_t current_primitive = 0;

	for (size_t i = 0; i < gltf_data->scene->nodes_count; i++)
	{
		LoadglTFNode(a_temp_allocator, a_mesh_op.shader_effects, a_list, a_upload_view, *gltf_data->scene->nodes[i], *model, current_node, current_primitive);
	}

	cgltf_free(gltf_data);

	AssetSlot asset;
	asset.hash = asset_hash;
	asset.path = FindOrCreateString(a_mesh_op.path);
	asset.model = model;
	OSAcquireSRWLockWrite(&s_asset_manager->asset_lock);
	s_asset_manager->asset_table.insert(asset.hash.full_hash, asset);
	OSReleaseSRWLockWrite(&s_asset_manager->asset_lock);
	model->asset_handle = AssetHandle(asset.hash.full_hash);
	return model;
}

const Model* Asset::LoadMeshFromMemory(const MeshLoadFromMemory& a_mesh_op, const RCommandList a_list, UploadBufferView& a_upload_view)
{
	const AssetHash asset_hash = CreateAssetHash(StringHash(a_mesh_op.name), ASSET_TYPE::MODEL);

	if (AssetSlot* slot = s_asset_manager->asset_table.find(asset_hash.full_hash))
	{
		return slot->model;
	}

	CreateMeshInfo mesh_info;
	mesh_info.vertices = a_mesh_op.vertices;
	mesh_info.indices = a_mesh_op.indices;
	OSAcquireSRWLockWrite(&s_asset_manager->asset_lock);
	//hack shit way, but a single mesh just has one primitive to draw.
	Model* model = ArenaAllocType(s_asset_manager->asset_arena, Model);
	model->linear_nodes = ArenaAllocArr(s_asset_manager->asset_arena, Model::Node, 1);
	model->primitives = ArenaAllocArr(s_asset_manager->asset_arena, Model::Primitive, 1);
	model->primitives->material = a_mesh_op.material;
	model->primitives->start_index = 0;
	model->primitives->index_count = a_mesh_op.indices.size();

	model->primitive_count = 1;
	model->root_node_count = 1;
	model->root_nodes = &model->linear_nodes[0];

	model->linear_nodes[0].child_count = 0;
	model->linear_nodes[0].mesh_handle = CreateMesh(a_list, mesh_info, a_upload_view);
	model->linear_nodes[0].primitives = model->primitives;
	model->linear_nodes[0].primitive_count = model->primitive_count;
	model->linear_nodes[0].transform = Float4x4Identity();

	AssetSlot asset;
	asset.hash = asset_hash;
	asset.path = FindOrCreateString(a_mesh_op.name);
	asset.model = model;

	s_asset_manager->asset_table.insert(asset.hash.full_hash, asset);
	OSReleaseSRWLockWrite(&s_asset_manager->asset_lock);

	model->asset_handle = AssetHandle(asset.hash.full_hash);
	return model;
}

const Model* Asset::FindModelByPath(const char* a_path)
{
	AssetHash asset_hash = CreateAssetHash(StringHash(a_path), ASSET_TYPE::MODEL);
	if (AssetSlot* slot = s_asset_manager->asset_table.find(asset_hash.full_hash))
	{
		return slot->model;
	}
	return nullptr;
}

const Model* Asset::FindModelByName(const char* a_name)
{
	AssetHash asset_hash = CreateAssetHash(StringHash(a_name), ASSET_TYPE::MODEL);
	if (AssetSlot* slot = s_asset_manager->asset_table.find(asset_hash.full_hash))
	{
		return slot->model;
	}
	return nullptr;
}

void Asset::FreeAsset(const AssetHandle a_asset_handle)
{
	AssetSlot* slot = s_asset_manager->asset_table.find(a_asset_handle.handle);

	switch (slot->hash.type)
	{
	case ASSET_TYPE::MODEL:
		//BBfreeArr(s_asset_manager->allocator, slot->model->linear_nodes);
		//BBfree(s_asset_manager->allocator, slot->model);
		break;
	case ASSET_TYPE::TEXTURE:
		//BBfree(s_asset_manager->allocator, slot->image);
		break;
	}

	OSAcquireSRWLockWrite(&s_asset_manager->asset_lock);
	s_asset_manager->asset_table.erase(a_asset_handle.handle);
	OSReleaseSRWLockWrite(&s_asset_manager->asset_lock);
}
