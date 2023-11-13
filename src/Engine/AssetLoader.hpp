#include "BBMemory.h"
#include "Rendererfwd.hpp"
#include "Utils/Slice.h"

namespace BB
{
	using AssetHandle = FrameworkHandle<struct AssetHandleTag>;
	constexpr const char MODELS_DIRECTORY[] = "Resources/models/";
	constexpr const char SHADERS_DIRECTORY[] = "Resources/shaders/";
	constexpr const char TEXTURE_DIRECTORY[] = "resources/textures/";

	class BBImage;
	class UploadBufferView;
	struct Image
	{
		uint32_t width;		//4
		uint32_t height;	//8

		RTexture gpu_image;	//16
		AssetHandle asset_handle;	//24
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

		enum class ASYNC_ASSET_TYPE : uint32_t
		{
			MODEL,
			TEXTURE
		};

		enum class ASYNC_LOAD_TYPE : uint32_t
		{
			DISK,
			MEMORY
		};

		struct TextureLoadFromMemory
		{
			const char* name;
			BBImage& image;
		};

		struct TextureLoadFromDisk
		{
			const char* name;
			const char* path;
		};

		struct MeshLoadFromDisk
		{
			const char* name;
			const char* path;
		};

		struct AsyncAsset
		{
			ASYNC_ASSET_TYPE asset_type;
			ASYNC_LOAD_TYPE load_type;
			union
			{
				TextureLoadFromMemory texture_memory;
				TextureLoadFromDisk texture_disk;
				MeshLoadFromDisk mesh_disk;
			};
		};

		void LoadASync(const BB::Slice<AsyncAsset> a_asyn_assets, const char* a_task_name = "upload asset task");

		const Image* LoadImage(const BB::BBImage& a_image, const char* a_name, const RCommandList a_list, UploadBufferView& a_upload_view);
		const Model* LoadglTFModel(Allocator a_temp_allocator, const char* a_Path);

		void FreeAsset(const AssetHandle a_asset_handle);
	};
}
