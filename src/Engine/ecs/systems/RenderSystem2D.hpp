#pragma once
#include "GPUBuffers.hpp"
#include "Rendererfwd.hpp"

namespace BB
{
    class RenderSystem2D
    {
    public:
        bool Init(MemoryArena& a_arena, const size_t a_gpu_buffer, const uint32_t back_buffer_count);
        void InitFontImage(const RDescriptorIndex a_image_index, const uint2 a_image_size, const uint2 a_glyph_count, const uint2 a_glyph_size);

        void RenderText(const RCommandList a_list, const uint2 a_text_size, const uint2 a_text_start_pos, const StringView a_string);

    private:
        uint2 m_image_size;
        uint2 m_glyph_size;
        RDescriptorIndex m_font_atlas;
        StaticArray<GPUBuffer> m_gpu_memory;
    };
}
