#include "AssetLoader.hpp"
#include "Storage/BBString.h"

#include "Storage/Hashmap.h"

#include "Math.inl"

#include "shared_common.hlsl.h"

#pragma warning(push, 0)
#define CGLTF_IMPLEMENTATION
#include "cgltf/cgltf.h"
#pragma warning (pop)

using namespace BB;

//crappy hash, don't care for now.
const uint64_t StringHash(const char* a_string)
{
	uint64_t hash = 5381;
	int c;

	while (c = *a_string++)
		hash = ((hash << 5) + hash) + c;

	return hash;
}

enum class AssetType : uint32_t
{
	MODEL
};

struct AssetSlot
{
	AssetType type;
	uint64_t hash;
	const char* path;
	union
	{
		Model* model;
	};
};

struct AssetManager
{
	FreelistAllocator_t allocator{ mbSize * 64, "asset manager allocator" };

	OL_HashMap<uint64_t, AssetSlot> asset_map{ allocator, 64 };
	OL_HashMap<uint64_t, char*> stringMap{ allocator, 128 };
};
static AssetManager s_AssetManager{};

using namespace BB;

char* Asset::FindOrCreateString(const char* a_string)
{
	const uint64_t t_StringHash = StringHash(a_string);
	char** t_StringPtr = s_AssetManager.stringMap.find(t_StringHash);
	if (t_StringPtr != nullptr)
		return *t_StringPtr;

	const uint32_t t_StringSize = static_cast<uint32_t>(strlen(a_string) + 1);
	char* t_String = BBnewArr(s_AssetManager.allocator, t_StringSize, char);
	memcpy(t_String, a_string, t_StringSize);
	t_String[t_StringSize - 1] = '\0';
	s_AssetManager.stringMap.emplace(t_StringHash, t_String);
	return t_String;
}

static inline void* GetAccessorDataPtr(const cgltf_accessor* a_Accessor)
{
	const size_t accessor_offset = a_Accessor->buffer_view->offset + a_Accessor->offset;
	return Pointer::Add(a_Accessor->buffer_view->buffer->data, accessor_offset);
}

static uint32_t ChildNodeCount(const cgltf_node& a_node)
{
	uint32_t count = a_node.children_count;
	for (size_t node_index = 0; node_index < a_node.children_count; node_index++)
	{
		const cgltf_node& node = *a_node.children[node_index];
		count += ChildNodeCount(node);
	}
	return count;
}

static void LoadglTFNode(const cgltf_node& a_node, Model& a_model, Vertex* a_vertices, uint32_t* a_indices, uint32_t& a_indices_index, uint32_t& a_node_index, uint32_t& a_mesh_index, uint32_t& a_prim_index)
{
	Model::Node& mod_node = a_model.linear_nodes[a_node_index++];

	if (a_node.has_matrix)
		memcpy(&mod_node.transform, a_node.matrix, sizeof(float4x4));
	else
		mod_node.transform = Float4x4Identity();

	if (a_node.mesh != nullptr)
	{
		BB_ASSERT(a_model.mesh_count > a_mesh_index, "Trying to load another mesh while all meshes should've been loaded");
		const cgltf_mesh& mesh = *a_node.mesh;
		Model::Mesh& mod_mesh = a_model.meshes[a_mesh_index];

		mod_node.mesh_index = a_mesh_index++;
		mod_mesh.primitive_offset = a_prim_index;
		mod_mesh.primitive_count = static_cast<uint32_t>(mesh.primitives_count);
	}
	else
		mod_node.mesh_index = MODEL_NO_MESH;

	mod_node.child_count = a_node.children_count;
	if (mod_node.child_count != 0)
	{
		mod_node.childeren = &a_model.linear_nodes[a_node_index]; //childeren are loaded linearly, i'm quite sure.
		for (size_t i = 0; i < mod_node.child_count; i++)
		{
			LoadglTFNode(*a_node.children[i], a_model, a_vertices, a_indices, a_indices_index, a_node_index, a_mesh_index, a_prim_index);
		}
	}
}

void Asset::LoadglTFModel(Allocator a_temp_allocator, const char* a_Path)
{
	cgltf_options gltf_option = {};
	cgltf_data* gltf_data = { 0 };

	BB_ASSERT(cgltf_parse_file(&gltf_option, a_Path, &gltf_data) == cgltf_result_success, "Failed to load glTF model, cgltf_parse_file.");

	cgltf_load_buffers(&gltf_option, gltf_data, a_Path);

	BB_ASSERT(cgltf_validate(gltf_data) == cgltf_result_success, "GLTF model validation failed!");


	uint32_t vertex_count = 0;
	uint32_t index_count = 0;
	uint32_t linear_node_count = 0;
	uint32_t primitive_count = 0;

	//Get the node count.
	for (size_t node_index = 0; node_index < gltf_data->nodes_count; node_index++)
	{
		const cgltf_node& node = gltf_data->nodes[node_index];
		linear_node_count += ChildNodeCount(node);
	}

	//get the index count
	for (size_t mesh_index = 0; mesh_index < gltf_data->meshes_count; mesh_index++)
	{
		const cgltf_mesh& mesh = gltf_data->meshes[mesh_index];
		primitive_count += mesh.primitives_count;

		for (size_t prim_index = 0; prim_index < mesh.primitives_count; prim_index++)
		{
			const cgltf_primitive& prim = mesh.primitives[prim_index];
			index_count += prim.indices->count;

			for (size_t attrib_index = 0; attrib_index < prim.attributes_count; attrib_index++)
			{
				const cgltf_attribute& attrib = prim.attributes[attrib_index];
				if (attrib.type == cgltf_attribute_type_position)
				{
					BB_ASSERT(attrib.data->type == cgltf_type_vec3, "GLTF position type is not a vec3!");
					vertex_count += static_cast<uint32_t>(attrib.data->count);
				}
			}
		}
	}

	Vertex* vertices = BBnewArr(a_temp_allocator, vertex_count, Vertex);
	uint32_t* indices = BBnewArr(a_temp_allocator, index_count, uint32_t);

	//optimize the memory space with one allocation for the entire model
	Model* model = BBnew(s_AssetManager.allocator, Model);

	model->mesh_count = gltf_data->meshes_count;
	model->meshes = BBnewArr(a_temp_allocator, model->mesh_count, Model::Mesh);
	model->primitive_count = primitive_count;
	model->primitives = BBnewArr(a_temp_allocator, model->primitive_count, Model::Primitive);

	model->linear_nodes = BBnewArr(a_temp_allocator, linear_node_count, Model::Node);

	//for now this is going to be the root node, which is 0.
	//This is not accurate on all GLTF models
	model->root_node_count = gltf_data->nodes_count;
	BB_ASSERT(model->root_node_count == 1, "not supporting more then 1 root node for gltf yet.");
	model->root_nodes = &model->linear_nodes[0];

	uint32_t current_index = 0;
	uint32_t current_node = 0;
	uint32_t current_mesh = 0;
	uint32_t current_primitive = 0;

	for (size_t i = 0; i < gltf_data->scene->nodes_count; i++)
	{
		LoadglTFNode(*gltf_data->scene->nodes[i], *model, vertices, indices, current_index, current_node, current_mesh, current_primitive);
	}
}

//Maybe use own allocators for this?
void LoadglTFModel(Allocator a_SystemAllocator, const char* a_Path)
{
	cgltf_options t_Options = {};
	cgltf_data* t_Data = { 0 };

	cgltf_result t_ParseResult = cgltf_parse_file(&t_Options, a_Path, &t_Data);

	BB_ASSERT(t_ParseResult == cgltf_result_success, "Failed to load glTF model, cgltf_parse_file.");

	cgltf_load_buffers(&t_Options, t_Data, a_Path);

	BB_ASSERT(cgltf_validate(t_Data) == cgltf_result_success, "GLTF model validation failed!");

	uint32_t t_IndexCount = 0;
	uint32_t t_VertexCount = 0;
	uint32_t t_LinearNodeCount = static_cast<uint32_t>(t_Data->nodes_count);
	uint32_t t_MeshCount = static_cast<uint32_t>(t_Data->meshes_count);
	uint32_t t_PrimitiveCount = 0;

	//Get the node count.
	for (size_t nodeIndex = 0; nodeIndex < t_Data->nodes_count; nodeIndex++)
	{
		const cgltf_node& t_Node = t_Data->nodes[nodeIndex];
		t_LinearNodeCount += GetChildNodeCount(t_Node);
	}

	//Get the sizes first for efficient allocation.
	for (size_t meshIndex = 0; meshIndex < t_Data->meshes_count; meshIndex++)
	{
		const cgltf_mesh& t_Mesh = t_Data->meshes[meshIndex];
		for (size_t primitiveIndex = 0; primitiveIndex < t_Mesh.primitives_count; primitiveIndex++)
		{
			++t_PrimitiveCount;
			const cgltf_primitive& t_Primitive = t_Mesh.primitives[primitiveIndex];
			t_IndexCount += static_cast<uint32_t>(t_Primitive.indices->count);

			for (size_t attrIndex = 0; attrIndex < t_Primitive.attributes_count; attrIndex++)
			{
				const cgltf_attribute& t_Attribute = t_Primitive.attributes[attrIndex];
				if (t_Attribute.type == cgltf_attribute_type_position)
				{
					BB_ASSERT(t_Attribute.data->type == cgltf_type_vec3, "GLTF position type is not a vec3!");
					t_VertexCount += static_cast<uint32_t>(t_Attribute.data->count);
				}
			}
		}
	}

	//Maybe allocate this all in one go
	Model::Mesh* t_Meshes = BBnewArr(
		a_SystemAllocator,
		t_MeshCount,
		Model::Mesh);
	a_Model.meshes = t_Meshes;
	a_Model.meshCount = t_MeshCount;

	Model::Primitive* t_Primitives = BBnewArr(
		a_SystemAllocator,
		t_PrimitiveCount,
		Model::Primitive);
	a_Model.primitives = t_Primitives;
	a_Model.primitiveCount = t_PrimitiveCount;

	Model::Node* t_LinearNodes = BBnewArr(
		a_SystemAllocator,
		t_LinearNodeCount,
		Model::Node);
	a_Model.linearNodes = t_LinearNodes;
	a_Model.linearNodeCount = t_LinearNodeCount;

	//Temporary stuff
	uint32_t* t_Indices = BBnewArr(
		a_SystemAllocator,
		t_IndexCount,
		uint32_t);
	Vertex* t_Vertices = BBnewArr(
		a_SystemAllocator,
		t_VertexCount,
		Vertex);

	for (size_t i = 0; i < t_VertexCount; i++) //temp solution to set color. Maybe permanent
	{
		t_Vertices[i].color.x = 1.0f;
		t_Vertices[i].color.y = 1.0f;
		t_Vertices[i].color.z = 1.0f;
	}

	uint32_t t_CurrentIndex = 0;
	uint32_t t_CurrentVertex = 0;

	uint32_t t_CurrentNode = 0;
	uint32_t t_CurrentMesh = 0;
	uint32_t t_CurrentPrimitive = 0;

	for (size_t i = 0; i < t_Data->scene->nodes_count; i++)
	{
		LoadglTFNode(t_TempAllocator, a_Model, *t_Data->scene->nodes[i], t_CurrentNode, t_CurrentMesh, t_CurrentPrimitive, t_Vertices, t_CurrentVertex, t_Indices, t_CurrentIndex);
	}

	//get it all in GPU buffers now.
	{
		const uint32_t t_VertexBufferSize = t_VertexCount * sizeof(Vertex);

		const UploadBufferChunk t_VertChunk = a_UploadBuffer.Alloc(t_VertexBufferSize);
		memcpy(t_VertChunk.memory, t_Vertices, t_VertexBufferSize);

		a_Model.vertexView = AllocateFromVertexBuffer(t_VertexBufferSize);

		RenderCopyBufferInfo t_CopyInfo{};
		t_CopyInfo.src = a_UploadBuffer.Buffer();
		t_CopyInfo.dst = a_Model.vertexView.buffer;
		t_CopyInfo.srcOffset = t_VertChunk.offset;
		t_CopyInfo.dstOffset = a_Model.vertexView.offset;
		t_CopyInfo.size = a_Model.vertexView.size;

		RenderBackend::CopyBuffer(a_CommandList, t_CopyInfo);
	}

	{
		const uint32_t t_IndexBufferSize = t_IndexCount * sizeof(uint32_t);

		const UploadBufferChunk t_IndexChunk = a_UploadBuffer.Alloc(t_IndexBufferSize);
		memcpy(t_IndexChunk.memory, t_Indices, t_IndexBufferSize);

		a_Model.indexView = AllocateFromIndexBuffer(t_IndexBufferSize);

		RenderCopyBufferInfo t_CopyInfo{};
		t_CopyInfo.src = a_UploadBuffer.Buffer();
		t_CopyInfo.dst = a_Model.indexView.buffer;
		t_CopyInfo.srcOffset = t_IndexChunk.offset;
		t_CopyInfo.dstOffset = a_Model.indexView.offset;
		t_CopyInfo.size = a_Model.indexView.size;

		RenderBackend::CopyBuffer(a_CommandList, t_CopyInfo);
	}

	cgltf_free(t_Data);
}