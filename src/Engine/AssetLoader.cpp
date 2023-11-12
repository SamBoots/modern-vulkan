#include "AssetLoader.hpp"
#include "Storage/BBString.h"

#include "Storage/Hashmap.h"

#include "BBImage.hpp"

#include "Math.inl"

#include "shared_common.hlsl.h"
#include "Renderer.hpp"

#include "BBIntrin.h"

BB_WARNINGS_OFF
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
BB_WARNINGS_ON

using namespace BB;

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

AssetHash CreateAssetHash(const uint64_t a_hash, const ASSET_TYPE a_type)
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
	FreelistAllocator_t allocator{ mbSize * 64, "asset manager allocator" };
	OL_HashMap<uint64_t, AssetSlot> asset_map{ allocator, 128 };

	LinearAllocator_t string_allocator{ mbSize * 16, "asset string allocator" };
	OL_HashMap<uint64_t, char*> string_map{ string_allocator, 1024 };
};
static AssetManager s_asset_manager{};

using namespace BB;

char* Asset::FindOrCreateString(const char* a_string)
{
	const uint64_t string_hash = StringHash(a_string);
	char** string_ptr = s_asset_manager.string_map.find(string_hash);
	if (string_ptr != nullptr)
		return *string_ptr;

	const uint32_t string_size = static_cast<uint32_t>(strlen(a_string) + 1);
	char* string = BBnewArr(s_asset_manager.string_allocator, string_size, char);
	memcpy(string, a_string, string_size);
	string[string_size - 1] = '\0';
	s_asset_manager.string_map.emplace(string_hash, string);
	return string;
}

const Image* Asset::LoadImage(const BB::BBImage& a_image, const char* a_name, const RCommandList a_list, UploadBufferView& a_upload_view)
{
	UploadImageInfo upload_image_info;
	upload_image_info.name = a_name;
	upload_image_info.pixels = a_image.GetPixels();
	upload_image_info.width = a_image.GetWidth();
	upload_image_info.height = a_image.GetWidth();
	upload_image_info.bit_count = a_image.GetBitCount();

	const RTexture gpu_image = UploadTexture(upload_image_info, a_list, a_upload_view);

	Image* image = BBnew(s_asset_manager.allocator, Image);
	image->width = upload_image_info.width;
	image->height = upload_image_info.height;
	image->gpu_image = gpu_image;

	const uint64_t hash = TurboCrappyImageHash(a_image.GetPixels(), static_cast<size_t>(image->width) + image->height + (a_image.GetBitCount() / 8));
	BB_ASSERT(hash != 0, "Image hashing failed");

	AssetSlot asset;
	asset.hash = CreateAssetHash(hash, ASSET_TYPE::TEXTURE);
	asset.path = nullptr; //memory loads have nullptr has path.
	asset.image = image;

	s_asset_manager.asset_map.insert(asset.hash.full_hash, asset);
	image->asset_handle = AssetHandle(asset.hash.full_hash);

	return image;
}

static inline void* GetAccessorDataPtr(const cgltf_accessor* a_Accessor)
{
	const size_t accessor_offset = a_Accessor->buffer_view->offset + a_Accessor->offset;
	return Pointer::Add(a_Accessor->buffer_view->buffer->data, accessor_offset);
}

static void LoadglTFNode(Allocator a_temp_allocator, const cgltf_node& a_node, Model& a_model, uint32_t& a_node_index)
{
	Model::Node& mod_node = a_model.linear_nodes[a_node_index++];

	if (a_node.has_matrix)
		memcpy(&mod_node.transform, a_node.matrix, sizeof(float4x4));
	else
		mod_node.transform = Float4x4Identity();

	if (a_node.mesh != nullptr)
	{
		const cgltf_mesh& mesh = *a_node.mesh;

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
		uint32_t* index_offset = indices;
		Vertex* vertex_offset = vertices;

		for (size_t prim_index = 0; prim_index < mesh.primitives_count; prim_index++)
		{
			const cgltf_primitive& prim = mesh.primitives[prim_index];

			//do images here.... 


			{	//get indices
				void* index_data = GetAccessorDataPtr(prim.indices);
				if (prim.indices->component_type == cgltf_component_type_r_32u)
				{
					Memory::Copy(&index_offset, index_data, prim.indices->count);
					index_offset += prim.indices->count;
				}
				else if (prim.indices->component_type == cgltf_component_type_r_16u)
				{
					for (size_t i = 0; i < prim.indices->count; i++)
						indices[i] = reinterpret_cast<uint16_t*>(index_data)[i];
					index_offset += prim.indices->count;
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
						vertex_offset[i].position.x = data_pos[0];
						vertex_offset[i].position.y = data_pos[1];
						vertex_offset[i].position.z = data_pos[2];

						data_pos = reinterpret_cast<const float*>(Pointer::Add(data_pos, attrib.data->stride));
						//increment the main vertices, jank.
						++vertex_offset;
					}
					break;
				case cgltf_attribute_type_normal:
					for (size_t i = 0; i < attrib.data->count; i++)
					{
						vertex_offset[i].normal.x = data_pos[0];
						vertex_offset[i].normal.y = data_pos[1];
						vertex_offset[i].normal.z = data_pos[2];

						data_pos = reinterpret_cast<const float*>(Pointer::Add(data_pos, attrib.data->stride));
					}
					break;
				case cgltf_attribute_type_texcoord:
					for (size_t i = 0; i < attrib.data->count; i++)
					{
						vertex_offset[i].uv.x = data_pos[0];
						vertex_offset[i].uv.y = data_pos[1];

						data_pos = reinterpret_cast<const float*>(Pointer::Add(data_pos, attrib.data->stride));
					}
					break;
				default:
					break;
				}
			}
		}

		for (size_t i = 0; i < vertex_count; i++)
		{
			vertices[i].color = { 1.f, 1.f, 1.f };
		}

		CreateMeshInfo create_mesh;
		create_mesh.vertices = Slice(vertices, vertex_count);
		create_mesh.indices = Slice(indices, index_count);

		mod_node.mesh_handle = CreateMesh(create_mesh);
	}
	else
		mod_node.mesh_handle = MeshHandle(BB_INVALID_HANDLE);

	mod_node.child_count = static_cast<uint32_t>(a_node.children_count);
	if (mod_node.child_count != 0)
	{
		mod_node.childeren = &a_model.linear_nodes[a_node_index]; //childeren are loaded linearly, i'm quite sure.
		for (size_t i = 0; i < mod_node.child_count; i++)
		{
			LoadglTFNode(a_temp_allocator, *a_node.children[i], a_model, a_node_index);
		}
	}
}

const Model* Asset::LoadglTFModel(Allocator a_temp_allocator, const char* a_path)
{
	cgltf_options gltf_option = {};
	cgltf_data* gltf_data = nullptr;

	BB_ASSERT(cgltf_parse_file(&gltf_option, a_path, &gltf_data) == cgltf_result_success, "Failed to load glTF model, cgltf_parse_file.");

	cgltf_load_buffers(&gltf_option, gltf_data, a_path);

	BB_ASSERT(cgltf_validate(gltf_data) == cgltf_result_success, "GLTF model validation failed!");

	const uint32_t linear_node_count = static_cast<uint32_t>(gltf_data->nodes_count);

	//optimize the memory space with one allocation for the entire model
	Model* model = BBnew(s_asset_manager.allocator, Model);
	model->linear_nodes = BBnewArr(s_asset_manager.allocator, linear_node_count, Model::Node);

	//for now this is going to be the root node, which is 0.
	//This is not accurate on all GLTF models
	model->root_node_count = static_cast<uint32_t>(gltf_data->scene->nodes_count);
	BB_ASSERT(model->root_node_count == 1, "not supporting more then 1 root node for gltf yet.");
	model->root_nodes = &model->linear_nodes[0];

	uint32_t current_node = 0;

	for (size_t i = 0; i < gltf_data->scene->nodes_count; i++)
	{
		LoadglTFNode(a_temp_allocator , *gltf_data->scene->nodes[i], *model, current_node);
	}

	cgltf_free(gltf_data);

	AssetSlot asset;
	asset.hash = CreateAssetHash(StringHash(a_path), ASSET_TYPE::MODEL);
	asset.path = FindOrCreateString(a_path);
	asset.model = model;

	s_asset_manager.asset_map.insert(asset.hash.full_hash, asset);

	model->asset_handle = AssetHandle(asset.hash.full_hash);
	return model;
}

void Asset::FreeAsset(const AssetHandle a_asset_handle)
{
	AssetSlot* slot = s_asset_manager.asset_map.find(a_asset_handle.handle);

	switch (slot->hash.type)
	{
	case ASSET_TYPE::MODEL:
		BBfreeArr(s_asset_manager.allocator, slot->model->linear_nodes);
		BBfree(s_asset_manager.allocator, slot->model);
		break;
	case ASSET_TYPE::TEXTURE:
		BBfree(s_asset_manager.allocator, slot->image);
		break;
	}

	s_asset_manager.asset_map.erase(a_asset_handle.handle);
}
