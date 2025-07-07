#pragma once
#include "GPUBuffers.hpp"
#include "Rendererfwd.hpp"

namespace BB
{
    class RenderSystem2D
    {
    public:
        bool Init(MemoryArena& a_arena);

        void RenderText(RCommandList& a_list, const uint2 a_text_size, const uint2 a_text_start_pos, const StringView a_string);

    private:
        RDescriptorIndex m_font_atlas;
        GPUBuffer m_gpu_memory;
    };
}
