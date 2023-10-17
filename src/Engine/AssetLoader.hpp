#include "BBMemory.h"

namespace BB
{
	using AssetHandle = FrameworkHandle<struct AssetHandleTag>;
	constexpr const char MODELS_DIRECTORY[] = "Resources/models/";
	constexpr const char SHADERS_DIRECTORY[] = "Resources/shaders/";
	constexpr const char TEXTURE_DIRECTORY[] = "resources/textures/";

	struct Model
	{
		struct Primitive
		{
			uint32_t index_start = 0;
			uint32_t index_count = 0;
			//RTexture baseColorIndex = BB_INVALID_HANDLE;
			//RTexture normalIndex = BB_INVALID_HANDLE;
		};

		struct Mesh
		{
			uint32_t primitive_offset = 0;
			uint32_t primitive_count = 0;
		};

		struct Node
		{
			float4x4 transform;
			Model::Node* childeren = nullptr;
			uint32_t child_count = 0;
			uint32_t mesh_index = MESH_INVALID_INDEX;
		};

		uint64_t vertex_offset;
		uint64_t index_offset;

		Node* nodes = nullptr;
		uint32_t node_count = 0;

		Mesh* meshes = nullptr;
		uint32_t mesh_count = 0;

		Primitive* primitives = nullptr;
		uint32_t primitive_count = 0;
	};

	namespace Asset
	{
		char* FindOrCreateString(const char* a_string);

		void LoadglTFModel(Allocator a_SystemAllocator, Model& a_Model, UploadBuffer& a_UploadBuffer, const CommandListHandle a_CommandList, const char* a_Path);

		Model LoadModel
	};
}