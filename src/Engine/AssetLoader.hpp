#pragma once
#include "Rendererfwd.hpp"
#include "Enginefwd.hpp"
#include "ecs/components/NameComponent.hpp"

// forward declare this, I don't know why that didn't actually work
#include "Storage/BBString.h"

//All this shit is jank, we return pointers of models and the model structs suck. Find a better way.

namespace BB
{
	using AssetHandle = FrameworkHandle<struct AssetHandleTag>;
	class BBImage;
	class UploadBufferView;

	// temp
	struct ImageInfo
	{
		RImage image;
		uint2 extent;
		int2 offset;
		uint16_t array_layers;
		uint16_t base_array_layer;
		uint32_t mip_layer;
	};

	constexpr size_t MAX_ASSET_NAME_SIZE = 64;
	using AssetString = StackString<MAX_ASSET_NAME_SIZE>;

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
			NameComponent name;
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

			size_t asset_upload_buffer_size = mbSize * 32;
			size_t max_textures = 1024;
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
			StringView name;
			BBImage* image;
		};

		struct TextureLoadFromDisk
		{
			StringView path;
			IMAGE_FORMAT format;
		};

		struct MeshLoadFromMemory
		{
			StringView name;
			// material def here....
			struct MeshLoad
			{
				size_t vertex_count;
				float3* positions;
				float3* normals;
				float2* uvs;
				float4* colors;
			} mesh_load;
			ConstSlice<uint32_t> indices;
			RDescriptorIndex base_albedo;
		};

		struct MeshLoadFromDisk
		{
			StringView path;
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

		ThreadTask LoadAssetsASync(MemoryArenaTemp a_temp_arena, const BB::Slice<Asset::AsyncAsset> a_asyn_assets);

		void LoadAssets(MemoryArena& a_temp_arena, const Slice<AsyncAsset> a_asyn_assets);

		const Image& LoadImageDisk(MemoryArena& a_temp_arena, const StringView& a_path, const IMAGE_FORMAT a_format);
		const Image& LoadImageMemory(MemoryArena& a_temp_arena, const BB::BBImage& a_image, const StringView& a_name);
		const Model& LoadglTFModel(MemoryArena& a_temp_arena, const MeshLoadFromDisk& a_mesh_op);
		const Model& LoadMeshFromMemory(MemoryArena& a_temp_arena, const MeshLoadFromMemory& a_mesh_op);

		bool ReadWriteTextureDeferred(const StringView& a_path, const ImageInfo& a_read_image_info);
		bool WriteImage(const StringView& a_path, const uint2 a_extent, const uint32_t a_channels, const void* a_pixels);
		unsigned char* LoadImageCPU(const StringView& a_path, int& a_width, int& a_height, int& a_bytes_per_pixel);
		void FreeImageCPU(void* a_pixels);

		const Model* FindModelByName(const char* a_name);
		const Image* FindImageByName(const char* a_name);

		void ShowAssetMenu(MemoryArena& a_temp_arena);

		void FreeAsset(const AssetHandle a_asset_handle);

		RDescriptorIndex GetWhiteTexture();
		RDescriptorIndex GetBlackTexture();
	};
}
