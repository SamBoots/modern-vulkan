#pragma once
#include "Rendererfwd.hpp"
#include "Utils/Slice.h"

// forward declare this, I don't know why that didn't actually work
#include "Storage/BBString.h"

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
		AssetHandle asset_handle; //24
	};

	struct Model
	{
		struct MaterialData
		{
			RTexture base_texture;		// 4
			RTexture normal_texture;	// 8
		};
		struct Primitive
		{
			//change this with material.
			uint32_t start_index;		// 4
			uint32_t index_count;		// 8
					
			MaterialHandle material;	// 16
			MaterialData material_data;	// 24

			const char* name;			// 32
		};

		struct Mesh
		{
			MeshHandle mesh_handle;				// 8
			StaticArray<Primitive> primitives;	// 24
		};

		struct Node
		{
			float3 translation;			// 12
			float3 scale;				// 24
			Quat rotation;				// 40
			Node* childeren;			// 48
			size_t child_count;			// 56
			Mesh* mesh;					// 64
			const char* name;			// 72
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
		};

		struct MeshLoadFromMemory
		{
			const char* name;
			// material def here....
			Slice<Vertex> vertices;
			Slice<uint32_t> indices;
			RTexture base_color;
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

		void LoadAssets(MemoryArena& a_memory_arena, const Slice<AsyncAsset> a_asyn_assets, const char* a_cmd_list_name = "upload asset task");

		const Image* LoadImageDisk(MemoryArena& a_temp_arena, const char* a_path, const RCommandList a_list, const uint64_t a_transfer_fence_value);
		const Image* LoadImageMemory(MemoryArena& a_temp_arena, const BB::BBImage& a_image, const char* a_name, const RCommandList a_list, const uint64_t a_transfer_fence_value);
		const Model* LoadglTFModel(MemoryArena& a_temp_arena, const MeshLoadFromDisk& a_mesh_op, const RCommandList a_list, const uint64_t a_transfer_fence_value);
		const Model* LoadMeshFromMemory(MemoryArena& a_temp_arena, const MeshLoadFromMemory& a_mesh_op, const RCommandList a_list, const uint64_t a_transfer_fence_value);

		bool WriteImage(const char* a_file_name, const uint32_t a_width, const uint32_t a_height, const uint32_t a_channels, const void* a_pixels);

		const Model* FindModelByPath(const char* a_path);
		const Model* FindModelByName(const char* a_name);

		void ShowAssetMenu(MemoryArena& a_temp_arena);

		void FreeAsset(const AssetHandle a_asset_handle);
	};
}
