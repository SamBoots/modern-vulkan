#include "BBMemory.h"
#include "Rendererfwd.hpp"
#include "Utils/Slice.h"

//All this shit is jank, we return pointers of models and the model structs suck. Find a better way.

namespace BB
{
	using AssetHandle = FrameworkHandle<struct AssetHandleTag>;
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
		struct Primitive
		{
			//change this with material.
			uint32_t start_index;	//4
			uint32_t index_count;	//8
			MaterialHandle material; //16
		};

		struct Node
		{
			float4x4 transform;			 //64
			MeshHandle mesh_handle;		 //72
			Model::Node* childeren;		 //80
			uint32_t child_count;		 //84
			Model::Primitive* primitives;//92
			uint32_t primitive_count;    //96
		};

		Primitive* primitives;
		uint32_t primitive_count;

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
			BBImage* image;
		};

		struct TextureLoadFromDisk
		{
			const char* name;
			const char* path;
		};

		struct MeshLoadFromMemory
		{
			const char* name;
			Slice<ShaderEffectHandle> shader_effects;
			Slice<Vertex> vertices;
			Slice<uint32_t> indices;
		};

		struct MeshLoadFromDisk
		{
			const char* name;
			const char* path;
			Slice<ShaderEffectHandle> shader_effects;
		};

		struct AsyncAsset
		{
			ASYNC_ASSET_TYPE asset_type;
			ASYNC_LOAD_TYPE load_type;
			union
			{
				TextureLoadFromMemory texture_memory;
				TextureLoadFromDisk texture_disk;
				MeshLoadFromMemory mesh_memory{};
				MeshLoadFromDisk mesh_disk;

			};
		};

		void LoadASync(const BB::Slice<AsyncAsset> a_asyn_assets, const char* a_task_name = "upload asset task");

		const Image* LoadImageDisk(const char* a_path, const char* a_name, const RCommandList a_list, UploadBufferView& a_upload_view);
		const Image* LoadImageMemory(const BB::BBImage& a_image, const char* a_name, const RCommandList a_list, UploadBufferView& a_upload_view);
		const Model* LoadglTFModel(Allocator a_temp_allocator, const MeshLoadFromDisk& a_mesh_op, const RCommandList a_list, UploadBufferView& a_upload_view);
		const Model* LoadMeshFromMemory(const MeshLoadFromMemory& a_mesh_op, const RCommandList a_list, UploadBufferView& a_upload_view);

		const Model* FindModelByPath(const char* a_path);
		const Model* FindModelByName(const char* a_name);

		void FreeAsset(const AssetHandle a_asset_handle);
	};
}
