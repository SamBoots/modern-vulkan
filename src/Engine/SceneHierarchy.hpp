#pragma once
#include "Common.h"
#include "Storage/Slotmap.h"
#include "Transform.hpp"
#include "Renderer.hpp"
#include "Storage/Array.h"
#include "Storage/BBString.h"
#include "AssetLoader.hpp"
#include "GPUBuffers.hpp"

namespace BB
{
	using SceneObjectHandle = FrameworkHandle<struct SceneObjectHandleTag>;
	constexpr SceneObjectHandle INVALID_SCENE_OBJ = SceneObjectHandle(BB_INVALID_HANDLE_64);
	class JsonParser;

	constexpr uint32_t DEFAULT_SCENE_OBJ_MAX = 512;
	constexpr uint32_t SCENE_OBJ_CHILD_MAX = 256;

	struct MeshDrawInfo
	{
		Mesh mesh;
		MaterialHandle material;
		uint32_t index_start;
		uint32_t index_count;
		RTexture base_texture;
		RTexture normal_texture;
	};

	struct SceneObject
	{
		const char* name;
		MeshDrawInfo mesh_info;

		LightHandle light_handle;

		TransformHandle transform;

		SceneObjectHandle parent;
		void AddChild(const SceneObjectHandle a_child)
		{
			BB_ASSERT(child_count < SCENE_OBJ_CHILD_MAX, "Too many children for a single scene object!");
			children[child_count++] = a_child;
		}
		size_t child_count;
		SceneObjectHandle children[SCENE_OBJ_CHILD_MAX];
	};

	struct SceneHierarchyCreateInfo
	{
		StringView name;
	};
	
	class SceneHierarchy
	{
	public:
		friend class Editor;
		void Init(MemoryArena& a_memory_arena, const StringView a_name, const uint32_t a_scene_obj_max = DEFAULT_SCENE_OBJ_MAX);
		static StaticArray<Asset::AsyncAsset> PreloadAssetsFromJson(MemoryArena& a_arena, const JsonParser& a_parsed_file);

		void DrawSceneHierarchy(const RCommandList a_list, const RTexture a_render_target, const uint2 a_draw_area_size, const int2 a_draw_area_offset);
		SceneObjectHandle CreateSceneObject(const float3 a_position, const char* a_name, const SceneObjectHandle a_parent = INVALID_SCENE_OBJ);
		SceneObjectHandle CreateSceneObjectMesh(const float3 a_position, const MeshDrawInfo& a_mesh_info, const char* a_name, const SceneObjectHandle a_parent = SceneObjectHandle(BB_INVALID_HANDLE_64));
		SceneObjectHandle CreateSceneObjectViaModel(const Model& a_model, const float3 a_position, const char* a_name, const SceneObjectHandle a_parent = INVALID_SCENE_OBJ);
		SceneObjectHandle CreateSceneObjectAsLight(const CreateLightInfo& a_light_create_info, const char* a_name, const SceneObjectHandle a_parent = INVALID_SCENE_OBJ);

		void SetView(const float4x4& a_view);
		void SetProjection(const float4x4& a_projection);

		void SetClearColor(const float3 a_clear_color) { m_clear_color = a_clear_color; }

		static RDescriptorLayout GetSceneDescriptorLayout();
	private:
		void AddToDrawList(const SceneObject& scene_object, const float4x4& a_transform);
		SceneObjectHandle CreateSceneObjectViaModelNode(const Model& a_model, const Model::Node& a_node, const SceneObjectHandle a_parent);
		void DrawSceneObject(const SceneObjectHandle a_scene_object, const float4x4& a_transform);

		struct DrawList
		{
			MeshDrawInfo* mesh_draw_call;
			ShaderTransform* transform;
			uint32_t size;
			uint32_t max_size;
		};
		struct PerFrameData
		{
			RFence fence;
			uint64_t fence_value;
			GPUUploadRingAllocator frame_allocator;
			StaticArray<GPULinearBuffer> uniform_buffer;
			Scene3DInfo scene_info;
		};

		DrawList m_draw_list;
		DescriptorAllocation m_scene_descriptor;

		PerFrameData m_per_frame;

		StringView m_scene_name;

		//TODO, maybe remember all the transforms from the previous frames?
		TransformPool m_transform_pool;
		StaticSlotmap<SceneObject, SceneObjectHandle> m_scene_objects;

		StaticSlotmap<Light, LightHandle> m_light_container;

		uint2 m_previous_draw_area;
		RTexture m_depth_image;

		uint32_t m_top_level_object_count;
		SceneObjectHandle* m_top_level_objects;

		float3 m_clear_color;
		RTexture m_skybox;
		MaterialHandle m_skybox_material;
	};
}
