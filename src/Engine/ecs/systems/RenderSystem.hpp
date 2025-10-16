#pragma once
#include "GPUBuffers.hpp"
#include "Rendererfwd.hpp"
#include "ecs/components/RenderComponent.hpp"
#include "ecs/components/RaytraceComponent.hpp"
#include "ecs/components/TransformComponents.hpp"
#include "ecs/components/LightComponent.hpp"

#include "UICanvas.hpp"

#include "Rendergraph.hpp"
#include "RenderOptions.hpp"

namespace BB
{
    class CommandPool;
	struct RenderSystemFrame
	{
		RDescriptorIndex render_target;
		RFence fence;
		uint64_t fence_value;
        CommandPool* pool;
	};

	class RenderSystem
	{
	public:
        friend class Editor;
        // temporary
        friend class EntityComponentSystem;
		void Init(MemoryArena& a_arena, const RenderOptions& a_options);

		void StartFrame(MemoryArena& a_per_frame_arena, const uint32_t a_max_ui_elements = 1024);
		RenderSystemFrame EndFrame(MemoryArena& a_per_frame_arena);
		void UpdateRenderSystem(MemoryArena& a_per_frame_arena, const WorldMatrixComponentPool& a_world_matrices, const RenderComponentPool& a_render_pool, const RaytraceComponentPool& a_raytrace_pool, const ConstSlice<LightComponent> a_lights);

		void Screenshot(const PathString& a_path) const;

        UICanvas& GetUIStage() { return m_ui_stage; }
        FontAtlas& GetDefaultFont() { return m_font_atlas; }
        const RenderOptions& GetOptions() { return m_options; }
        void SetOptions(const RenderOptions& a_options);
        void AddLinesToFrame(const ConstSlice<Line> a_lines);
		void SetView(const float4x4& a_view, const float3& a_view_position);
		void SetProjection(const float4x4& a_projection, const float a_near_plane);

        float4x4 GetProjection() const {return m_graph_system.GetConstGlobalData().scene_info.proj; }
        float4x4 GetView() const {return m_graph_system.GetConstGlobalData().scene_info.view; }

	private:
        void RasterFrame(MemoryArena& a_per_frame_arena, const uint3 a_render_target_size, const IMAGE_FORMAT a_render_target_format, const RG::ResourceHandle a_matrix_buffer, const ConstSlice<LightComponent> a_lights);
        void RaytraceFrame(MemoryArena& a_per_frame_arena, const RG::ResourceHandle a_matrix_buffer, const RaytraceComponentPool& a_raytrace_pool);

        RG::RenderGraphSystem m_graph_system;
        RG::RenderGraph* m_cur_graph;
        RG::ResourceHandle m_final_image;

        StaticArray<Line> m_lines;

        RenderOptions m_options;
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
        } m_raytrace_data;

        // old shit

        UICanvas m_ui_stage;
        FontAtlas m_font_atlas;
	};
}
