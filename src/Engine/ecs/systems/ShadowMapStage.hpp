#pragma once
#include "RenderStagesfwd.hpp"

#include "ecs/components/TransformComponents.hpp"
#include "ecs/components/LightComponent.hpp"

namespace BB
{
    constexpr uint32_t DEPTH_IMAGE_SIZE_W_H = 4096;

    class ShadowMapStage
    {
    public:
        void Init(MemoryArena& a_arena, const uint32_t a_back_buffer_count);
        void ExecutePass(const RCommandList a_list, const uint32_t a_frame_index, const uint2 a_shadow_map_resolution, const DrawList& a_draw_list, const ConstSlice<LightComponent> a_lights);
        void UpdateConstantBuffer(const uint32_t a_frame_index, Scene3DInfo& a_scene_3d_info) const;
    private:
        struct PerFrame
        {
            RImage image;
            RDescriptorIndex descriptor_index;
            StaticArray<RImageView> render_pass_views;
        };

        StaticArray<PerFrame> m_per_frame;
        MasterMaterialHandle m_shadowmap_material;
    };
}
