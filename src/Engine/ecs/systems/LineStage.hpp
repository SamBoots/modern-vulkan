#pragma once
#include "RenderStagesfwd.hpp"

namespace BB
{
    constexpr uint32_t LINE_MAX = 64 * 64;

    class LineStage
    {
    public:
        void Init(MemoryArena& a_arena, const uint32_t a_back_buffer_count, const uint32_t a_line_max);
        void ExecutePass(const RCommandList a_list, const uint32_t a_frame_index, const uint2 a_draw_area, const RImageView a_render_target, const RImageView a_depth_buffer);
        void AddLines(const uint32_t a_frame_index, const ConstSlice<Line> a_lines);
    private:
        struct PerFrame
        {
            uint32_t lines;
            WriteableGPUBufferView vertex_view;
        };

        StaticArray<PerFrame> m_per_frame;
        MasterMaterialHandle m_line_material;
    };
}
