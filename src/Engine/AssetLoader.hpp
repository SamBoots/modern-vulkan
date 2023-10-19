#include "BBMemory.h"

namespace BB
{
	using AssetHandle = FrameworkHandle<struct AssetHandleTag>;
	constexpr const char MODELS_DIRECTORY[] = "Resources/models/";
	constexpr const char SHADERS_DIRECTORY[] = "Resources/shaders/";
	constexpr const char TEXTURE_DIRECTORY[] = "resources/textures/";

	constexpr uint32_t MODEL_NO_MESH;
	struct Model
	{
		struct Primitive
		{
			uint32_t index_start;
			uint32_t index_count;
			//RTexture baseColorIndex = BB_INVALID_HANDLE;
			//RTexture normalIndex = BB_INVALID_HANDLE;
		};

		struct Mesh
		{
			MeshHandle mesh_handle;

			uint32_t primitive_offset;
			uint32_t primitive_count;
		};

		struct Node
		{
			float4x4 transform;
			Model::Node* childeren;
			uint32_t child_count;
			uint32_t mesh_index;
		};

		Node* linear_nodes;
		Node* root_nodes;
		uint32_t root_node_count;

		Mesh* meshes;
		uint32_t mesh_count;

		Primitive* primitives;
		uint32_t primitive_count;
	};

	namespace Asset
	{
		char* FindOrCreateString(const char* a_string);

		void LoadglTFModel(const char* a_Path);
	};
}