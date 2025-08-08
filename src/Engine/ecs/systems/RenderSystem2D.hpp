#pragma once
#include "GPUBuffers.hpp"
#include "Rendererfwd.hpp"
#include "Enginefwd.hpp"

namespace BB
{
    struct Glyph
    {
        int2 pos;
        int2 extent;
        int advance;
        float y_offset;
    };

    struct FontAtlas
    {
        AssetHandle asset;
        RImage image;
        RDescriptorIndex desc_index;
        uint2 extent;
        int char_start;
        int char_count;
        Glyph* glyphs;
        float pixel_height;
        
        // temp
        unsigned char* bitmap;
    };

    FontAtlas CreateFontAtlas(MemoryArena& a_arena, const PathString& a_font_path, const float a_pixel_height, const int a_first_char);
    bool FontAtlasWriteImage(const PathString& a_path, const FontAtlas& a_atlas);

    bool RenderText(const FontAtlas& a_font_atlas, const RCommandList a_list, GPUUploadRingAllocator& a_ring_buffer, GPULinearBuffer& a_frame_buffer, const float2 a_text_size, const float2 a_text_start_pos, const StringView a_string);
}
