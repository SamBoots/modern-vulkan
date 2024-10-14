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

	struct LightCreateInfo
	{
		LIGHT_TYPE light_type;      // 4
		float3 color;               // 16
		float3 pos;                 // 28
		float specular_strength;    // 32

		float radius_constant;      // 36
		float radius_linear;        // 40
		float radius_quadratic;     // 44

		float3 spotlight_direction; // 56
		float cutoff_radius;        // 60
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

	class SceneHierarchy
	{
	public:
		friend class Editor;
		void Init(MemoryArena& a_memory_arena, const uint32_t a_back_buffers, const StringView a_name, const uint32_t a_scene_obj_max = DEFAULT_SCENE_OBJ_MAX);
		static StaticArray<Asset::AsyncAsset> PreloadAssetsFromJson(MemoryArena& a_arena, const JsonParser& a_parsed_file);

		void DrawSceneHierarchy(const RCommandList a_list, const RTexture a_render_target, const uint32_t a_back_buffer_index, const uint2 a_draw_area_size, const int2 a_draw_area_offset);
		SceneObjectHandle CreateSceneObject(const float3 a_position, const char* a_name, const SceneObjectHandle a_parent = INVALID_SCENE_OBJ);
		SceneObjectHandle CreateSceneObjectMesh(const float3 a_position, const MeshDrawInfo& a_mesh_info, const char* a_name, const SceneObjectHandle a_parent = SceneObjectHandle(BB_INVALID_HANDLE_64));
		SceneObjectHandle CreateSceneObjectViaModel(const Model& a_model, const float3 a_position, const char* a_name, const SceneObjectHandle a_parent = INVALID_SCENE_OBJ);
		SceneObjectHandle CreateSceneObjectAsLight(const LightCreateInfo& a_light_create_info, const char* a_name, const SceneObjectHandle a_parent = INVALID_SCENE_OBJ);

		void SetView(const float4x4& a_view);
		void SetProjection(const float4x4& a_projection);

		void SetClearColor(const float3 a_clear_color) { m_clear_color = a_clear_color; }
		void IncrementNextFenceValue(RFence* a_out_fence, uint64_t* a_out_value) 
		{ 
			*a_out_fence = m_fence;  
			*a_out_value = m_next_fence_value++;
		}

		static RDescriptorLayout GetSceneDescriptorLayout();
	private:
		struct PerFrameData
		{
			uint64_t fence_value;
			DescriptorAllocation scene_descriptor;
			// i want this to be uniform but hlsl is giga cringe
			GPULinearBuffer storage_buffer;
			struct ShadowMap
			{
				RTexture texture;
				uint32_t array_count;
			} shadow_map;
		};

		void SkyboxPass(const PerFrameData& pfd, const RCommandList a_list, const RTexture a_render_target, const uint32_t a_back_buffer_index, const uint2 a_draw_area_size, const int2 a_draw_area_offset);
		void ResourceUploadPass(PerFrameData& pfd, const RCommandList a_list);
		void ShadowMapPass(const PerFrameData& pfd, const RCommandList a_list, const uint2 a_shadow_map_resolution);
		void RenderPass(const PerFrameData& pfd, const RCommandList a_list, const RTexture a_render_target, const uint32_t a_back_buffer_index, const uint2 a_draw_area_size, const int2 a_draw_area_offset);

		void AddToDrawList(const SceneObject& scene_object, const float4x4& a_transform);
		SceneObjectHandle CreateSceneObjectViaModelNode(const Model& a_model, const Model::Node& a_node, const SceneObjectHandle a_parent);
		void DrawSceneObject(const SceneObjectHandle a_scene_object, const float4x4& a_transform);

		LightHandle CreateLight(const LightCreateInfo& a_light_info);
		Light& GetLight(const LightHandle a_light) const;
		void FreeLight(const LightHandle a_light);

		struct DrawList
		{
			MeshDrawInfo* mesh_draw_call;
			ShaderTransform* transform;
			uint32_t size;
			uint32_t max_size;
		};

		Scene3DInfo m_scene_info;
		DrawList m_draw_list;

		RFence m_fence;
		uint64_t m_next_fence_value;
		uint64_t m_last_completed_fence_value;
		GPUUploadRingAllocator m_upload_allocator;
		StaticArray<PerFrameData> m_per_frame;

		StringView m_scene_name;

		//TODO, maybe remember all the transforms from the previous frames?
		TransformPool m_transform_pool;
		StaticSlotmap<SceneObject, SceneObjectHandle> m_scene_objects;

		StaticSlotmap<Light, LightHandle> m_light_container;
		StaticSlotmap<LightProjectionView, LightHandle> m_light_projection_view;

		uint2 m_previous_draw_area;
		RTexture m_depth_image;

		uint32_t m_top_level_object_count;
		SceneObjectHandle* m_top_level_objects;

		float3 m_clear_color;
		RTexture m_skybox;
		MaterialHandle m_skybox_material;
		MaterialHandle m_shadowmap_material;
	};
}
