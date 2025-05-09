#pragma once
#include "RenderStagesfwd.hpp"

#include "ecs/components/TransformComponents.hpp"
#include "ecs/components/LightComponent.hpp"


namespace BB
{
    class RasterMeshStage
    {
    public:
        void Init(MemoryArena& a_arena, const uint2 a_render_target_extent, const uint32_t a_back_buffer_count);
        void ExecutePass(const RCommandList a_list, const uint32_t a_frame_index, const uint2 a_draw_area_size, const DrawList& a_draw_list, const RImageView a_render_target, const RImageView a_render_target_bright);
        RImageView GetDepth(const uint32_t a_frame_index) const
        {
            return m_per_frame[a_frame_index].depth_image_view;
        }
    private:
        struct PerFrame
        {
            RImage depth_image;
            RImageView depth_image_view;
            uint2 depth_extent;
        };
        void CreateDepthImages(PerFrame& a_frame, const uint2 a_extent);

        StaticArray<PerFrame> m_per_frame;
    };
}
