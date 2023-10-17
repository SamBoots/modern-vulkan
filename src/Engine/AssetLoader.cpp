#include "AssetLoader.hpp"
#include "Storage/BBString.h"

#include "Storage/Hashmap.h"

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
	IMAGE
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