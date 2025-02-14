#pragma once
#include "Rendererfwd.hpp"
#include "Enginefwd.hpp"

// forward declare this, I don't know why that didn't actually work
#include "Storage/BBString.h"

//All this shit is jank, we return pointers of models and the model structs suck. Find a better way.

namespace BB
{
	using AssetHandle = FrameworkHandle<struct AssetHandleTag>;
	class BBImage;
	class UploadBufferView;

	// used this to forward declare it
	class PathString : public StackString<MAX_PATH_SIZE>
	{
	public:
		using StackString<MAX_PATH_SIZE>::StackString;
	};

	struct Image
	{
		uint32_t width;		//4
		uint32_t height;	//8

		RImage gpu_image;   //16
		AssetHandle asset_handle; //24
		RDescriptorIndex descriptor_index;
	};

	struct Model
	{
		struct MaterialData
		{
			MasterMaterialHandle material;      // 8
			MeshMetallic mesh_metallic;			// 40
		};
		struct Primitive
		{
			//change this with material.
			uint32_t start_index;				// 4
			uint32_t index_count;				// 8

			MaterialData material_data;			// 32
		};

		struct Mesh
		{
			BB::Mesh mesh;						// 16
			StaticArray<Primitive> primitives;	// 32
		};

		struct Node
		{
			float3 translation;
			float3 scale;
			float3x3 rotation;
			Node* childeren;
			size_t child_count;
			Mesh* mesh;
			const char* name;
		};

		StaticArray<Mesh> meshes;

		Node* linear_nodes;
		uint32_t* root_node_indices;
		uint32_t root_node_count;
		AssetHandle asset_handle;
	};

	namespace Asset
	{
		constexpr size_t ASSET_COUNT_STANDARD = 512;
		constexpr size_t STRING_ENTRY_COUNT_STANDARD = 2024;

		struct AssetManagerInitInfo
		{
			uint32_t asset_count = ASSET_COUNT_STANDARD;
			uint32_t string_entry_count = STRING_ENTRY_COUNT_STANDARD;
		};

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
			const char* path;
			IMAGE_FORMAT format;
		};

		struct MeshLoadFromMemory
		{
			const char* name;
			// material def here....
			Slice<Vertex> vertices;
			Slice<uint32_t> indices;
			RDescriptorIndex base_albedo;
		};

		struct MeshLoadFromDisk
		{
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
				MeshLoadFromMemory mesh_memory{};
				MeshLoadFromDisk mesh_disk;
			};
		};

		void InitializeAssetManager(const AssetManagerInitInfo& a_init_info);

		void Update();

		StringView FindOrCreateString(const char* a_string);
		StringView FindOrCreateString(const char* a_string, const size_t a_string_size);
		StringView FindOrCreateString(const StringView& a_view);

		ThreadTask LoadAssetsASync(MemoryArenaTemp a_temp_arena, const BB::Slice<Asset::AsyncAsset> a_asyn_assets);
		struct LoadedAssetInfo
		{
			StringView name;
			ASYNC_ASSET_TYPE type;
		};
		Slice<LoadedAssetInfo> LoadAssets(MemoryArena& a_temp_arena, const Slice<AsyncAsset> a_asyn_assets);

		const StringView LoadImageDisk(MemoryArena& a_temp_arena, const char* a_path, const IMAGE_FORMAT a_format);
		const StringView LoadImageMemory(MemoryArena& a_temp_arena, const BB::BBImage& a_image, const char* a_name);
		const StringView LoadglTFModel(MemoryArena& a_temp_arena, const MeshLoadFromDisk& a_mesh_op);
		const StringView LoadMeshFromMemory(MemoryArena& a_temp_arena, const MeshLoadFromMemory& a_mesh_op);

		bool ReadWriteTextureDeferred(const PathString& a_path, const ImageInfo& a_read_image_info);
		bool WriteImage(const PathString& a_path, const uint2 a_extent, const uint32_t a_channels, const void* a_pixels);
		unsigned char* LoadImageCPU(const PathString& a_path, int& a_width, int& a_height, int& a_bytes_per_pixel);
		void FreeImageCPU(void* a_pixels);

		const Model* FindModelByName(const char* a_name);
		const Image* FindImageByName(const char* a_name);

		void ShowAssetMenu(MemoryArena& a_temp_arena);

		void FreeAsset(const AssetHandle a_asset_handle);
	};
}
