#include "BBMemory.h"
#include "Rendererfwd.hpp"

namespace BB
{
	using AssetHandle = FrameworkHandle<struct AssetHandleTag>;
	constexpr const char MODELS_DIRECTORY[] = "Resources/models/";
	constexpr const char SHADERS_DIRECTORY[] = "Resources/shaders/";
	constexpr const char TEXTURE_DIRECTORY[] = "resources/textures/";

	class BBImage;
	struct Image
	{
		uint32_t width;		//4
		uint32_t height;	//8
		const void* pixels;	//16

		RImage gpu_image;	//24
		AssetHandle asset_handle;	//32
	};

	struct Model
	{
		struct Node
		{
			float4x4 transform;		//64
			MeshHandle mesh_handle; //72
			Model::Node* childeren; //80
			uint32_t child_count;	//84
		};

		Node* linear_nodes;
		Node* root_nodes;
		uint32_t root_node_count;

		AssetHandle asset_handle;
	};

	namespace Asset
	{
		char* FindOrCreateString(const char* a_string);

		const Image* LoadImage(const BB::BBImage& a_image, const char* a_name);
		const Model* LoadglTFModel(Allocator a_temp_allocator, const char* a_Path);
	};
}
