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

	struct LightCreateInfo
	{
		LIGHT_TYPE light_type;      // 4
		float3 color;               // 16
		float3 pos;                 // 28
		float specular_strength;    // 32

		float radius_constant;      // 36
		float radius_linear;        // 40
		float radius_quadratic;     // 44

		float3 direction;           // 56
		float cutoff_radius;        // 60
	};

	struct MeshDrawInfo
	{
		Mesh mesh;
		MasterMaterialHandle master_material;
		MaterialHandle material;
		MeshMetallic material_data;
		uint32_t index_start;
		uint32_t index_count;
		bool material_dirty;
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

	struct SceneMeshCreateInfo
	{
		Mesh mesh;
		uint32_t index_start;
		uint32_t index_count;
		MasterMaterialHandle master_material;
		MeshMetallic material_data;
	};

	class SceneHierarchy
	{
	public:
		friend class Editor;
		void Init(MemoryArena& a_memory_arena, const uint32_t a_back_buffers, const StringView a_name, const uint32_t a_scene_obj_max = DEFAULT_SCENE_OBJ_MAX);
		static StaticArray<Asset::AsyncAsset> PreloadAssetsFromJson(MemoryArena& a_arena, const JsonParser& a_parsed_file);

		void DrawSceneHierarchy(const RCommandList a_list, const RImageView a_render_target_view, const uint32_t a_back_buffer_index, const uint2 a_draw_area_size, const int2 a_draw_area_offset);
		SceneObjectHandle CreateSceneObject(const float3 a_position, const char* a_name, const SceneObjectHandle a_parent = INVALID_SCENE_OBJ);
		SceneObjectHandle CreateSceneObjectMesh(const float3 a_position, const SceneMeshCreateInfo& a_mesh_info, const char* a_name, const SceneObjectHandle a_parent = INVALID_SCENE_OBJ);
		SceneObjectHandle CreateSceneObjectViaModel(const Model& a_model, const float3 a_position, const char* a_name, const SceneObjectHandle a_parent = INVALID_SCENE_OBJ);
		SceneObjectHandle CreateSceneObjectAsLight(const LightCreateInfo& a_light_create_info, const char* a_name, const SceneObjectHandle a_parent = INVALID_SCENE_OBJ);

		void SetView(const float4x4& a_view, const float3& a_view_position);
		void SetProjection(const float4x4& a_projection);

		bool ToggleSkipSkyboxPass()
		{
			return m_options.skip_skybox = !m_options.skip_skybox;
		}

		bool ToggleSkipShadowMappingPass()
		{
			return m_options.skip_shadow_mapping = !m_options.skip_shadow_mapping;
		}

		bool ToggleSkipObjectRenderingPass()
		{
			return m_options.skip_object_rendering = !m_options.skip_object_rendering;
		}

		bool ToggleSkipBloomPass()
		{
			return m_options.skip_bloom = !m_options.skip_bloom;
		}

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
			uint2 previous_draw_area;
			uint64_t fence_value;
			DescriptorAllocation scene_descriptor;

			// scene data
			GPUStaticCPUWriteableBuffer scene_buffer;
			// I want this to be uniform but hlsl is giga cringe
			GPULinearBuffer storage_buffer;

			struct Bloom
			{
				RImage image;
				RDescriptorIndex descriptor_index_0;
				RDescriptorIndex descriptor_index_1;
				uint2 resolution;
			};
			Bloom bloom;
			
			RImage depth_image;
			RImageView depth_image_view;
			struct ShadowMap
			{
				RImage image;
				RDescriptorIndex descriptor_index;
				StaticArray<RImageView> render_pass_views;
			} shadow_map;
		};

		struct PostFXOptions
		{
			float bloom_strength;
			float bloom_scale;
		};
		PostFXOptions m_postfx;

		PostFXOptions& GetPostFXOptions() { return m_postfx; }

		void UpdateConstantBuffer(PerFrameData& a_pfd, const RCommandList a_list, const uint2 a_draw_area_size);
		void SkyboxPass(const PerFrameData& a_pfd, const RCommandList a_list, const RImageView a_render_target, const uint2 a_draw_area_size, const int2 a_draw_area_offset);
		void ResourceUploadPass(PerFrameData& a_pfd, const RCommandList a_list);
		void ShadowMapPass(const PerFrameData& a_pfd, const RCommandList a_list, const uint2 a_shadow_map_resolution);
		void GeometryPass(const PerFrameData& a_pfd, const RCommandList a_list, const RImageView a_render_target, const uint2 a_draw_area_size, const int2 a_draw_area_offset);
		void BloomPass(const PerFrameData& a_pfd, const RCommandList a_list, const RImageView a_render_target, const uint2 a_draw_area_size, const int2 a_draw_area_offset);

		void AddToDrawList(const SceneObject& scene_object, const float4x4& a_transform);
		SceneObjectHandle CreateSceneObjectViaModelNode(const Model& a_model, const Model::Node& a_node, const SceneObjectHandle a_parent);
		void DrawSceneObject(const SceneObjectHandle a_scene_object, const float4x4& a_transform, const RCommandList a_list, const PerFrameData& a_pfd);

		LightHandle CreateLight(const LightCreateInfo& a_light_info);
		float4x4 CalculateLightProjectionView(const float3 a_pos, const float a_near, const float a_far) const;
		Light& GetLight(const LightHandle a_light) const;
		void FreeLight(const LightHandle a_light);

		struct DrawList
		{
			MeshDrawInfo* mesh_draw_call;
			ShaderTransform* transform;
			uint32_t size;
			uint32_t max_size;
		};

		struct Options
		{
			bool skip_skybox;
			bool skip_shadow_mapping;
			bool skip_object_rendering;
			bool skip_bloom;
		} m_options;

		enum class SCENE_OBJ_DIRTY_TYPE
		{
			MATERIAL
		};

		struct SceneObjDirty
		{
			SceneObjectHandle obj;
			MaterialHandle material;
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
		StaticSlotmap<float4x4, LightHandle> m_light_projection_view;

		uint32_t m_top_level_object_count;
		SceneObjectHandle* m_top_level_objects;

		float3 m_clear_color;
		RImage m_skybox;
		RDescriptorIndex m_skybox_descriptor_index;
		MasterMaterialHandle m_skybox_material;
		MasterMaterialHandle m_shadowmap_material;
		MasterMaterialHandle m_gaussian_material;
	};
}
