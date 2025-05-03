#pragma once
#include "RenderStagesfwd.hpp"

namespace BB
{
    constexpr uint32_t DEPTH_IMAGE_SIZE_W_H = 4096;

    class LineStage
    {
    public:
        void Init(MemoryArena& a_arena, const uint32_t a_back_buffer_count, const uint32_t a_line_max);
        void ExecutePass(const RCommandList a_list, const uint32_t a_frame_index, const ConstSlice<Line> a_lines);
    private:
        struct PerFrame
        {
            WriteableGPUBufferView vertex_view;
        };

        StaticArray<PerFrame> m_per_frame;
        MasterMaterialHandle m_line_material;
    };
}
