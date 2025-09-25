#pragma once
#include "GPUBuffers.hpp"
#include "Rendererfwd.hpp"
#include "ecs/components/RenderComponent.hpp"
#include "ecs/components/RaytraceComponent.hpp"
#include "ecs/components/TransformComponents.hpp"
#include "ecs/components/LightComponent.hpp"

#include "UICanvas.hpp"

#include "Rendergraph.hpp"

namespace BB
{
	struct RenderSystemFrame
	{
		RDescriptorIndex render_target;
		RFence fence;
		uint64_t fence_value;
	};

	class RenderSystem
	{
	public:
        friend class Editor;
        // temporary
        friend class EntityComponentSystem;
		void Init(MemoryArena& a_arena, const uint32_t a_back_buffer_count, const uint2 a_render_target_size);

		void StartFrame(MemoryArena& a_per_frame_arena, const uint32_t a_max_ui_elements = 1024);
		RenderSystemFrame EndFrame(const RCommandList a_list, const IMAGE_LAYOUT a_current_layout);
		void UpdateRenderSystem(MemoryArena& a_per_frame_arena, const RCommandList a_list, const uint2 a_draw_area, const WorldMatrixComponentPool& a_world_matrices, const RenderComponentPool& a_render_pool, const RaytraceComponentPool& a_raytrace_pool, const ConstSlice<LightComponent> a_lights);

		void Resize(const uint2 a_new_extent, const bool a_force = false);
		void ResizeNewFormat(const uint2 a_render_target_size, const IMAGE_FORMAT a_render_target_format);
		void Screenshot(const PathString& a_path) const;

        UICanvas& GetUIStage() { return m_ui_stage; }
        FontAtlas& GetDefaultFont() {return m_font_atlas;}

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

		uint2 GetRenderTargetExtent() const
		{
			return m_final_image_extent;
		}

		void SetView(const float4x4& a_view, const float3& a_view_position);
		void SetProjection(const float4x4& a_projection, const float a_near_plane);

        float4x4 GetProjection() const {return m_graph_system.GetConstGlobalData().scene_info.proj; }
        float4x4 GetView() const {return m_graph_system.GetConstGlobalData().scene_info.view; }

	private:
        RG::RenderGraphSystem m_graph_system;
        RG::RenderGraph* m_cur_graph;

        uint2 m_final_image_extent;
        IMAGE_FORMAT m_final_image_format;
        RG::ResourceHandle m_final_image;
        uint32_t m_current_frame;
        uint32_t m_frame_count;

        RDescriptorIndex m_skybox_descriptor_index;
        RSampler m_skybox_sampler;
        RDescriptorIndex m_skybox_sampler_index;
        RImage m_skybox;

        MasterMaterialHandle m_skybox_material;
        MasterMaterialHandle m_shadowmap_material;
        MasterMaterialHandle m_glyph_material;
        MasterMaterialHandle m_line_material;
        MasterMaterialHandle m_gaussian_material;

        struct RaytraceData
        {
            GPULinearBuffer acceleration_structure_buffer;

			struct TopLevel
			{
				struct BuildInfo
				{
					GPULinearBuffer build_buffer;
					GPUAddress build_address;
					void* build_mapped;
				} build_info;

				RAccelerationStruct accel_struct;
				GPUBufferView accel_buffer_view;
				uint32_t build_size;
				uint32_t scratch_size;
				uint32_t scratch_update;
				bool must_update;
				bool must_rebuild;
			} top_level;

        } m_raytrace_data;

		void BuildTopLevelAccelerationStructure(MemoryArena& a_per_frame_arena, const RCommandList a_list, const ConstSlice<AccelerationStructureInstanceInfo> a_instances);

		struct Options
		{
			bool skip_skybox;
			bool skip_shadow_mapping;
			bool skip_object_rendering;
			bool skip_bloom;
		} m_options;

        // old shit

        UICanvas m_ui_stage;
        FontAtlas m_font_atlas;
	};
}
