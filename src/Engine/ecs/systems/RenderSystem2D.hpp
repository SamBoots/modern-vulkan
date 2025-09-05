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
        RImage image;
        RDescriptorIndex image_index;
        uint2 extent;
        int char_start;
        int char_count;
        Glyph* glyphs;
        float pixel_height;
        AssetHandle asset;

        float text_height;
        
        // temp
        unsigned char* bitmap;
        uint32_t current_frame;
        RDescriptorIndex per_frame[3];
        MasterMaterialHandle material;
    };

    FontAtlas CreateFontAtlas(MemoryArena& a_arena, const PathString& a_font_path, const float a_pixel_height, const int a_first_char);
    bool FontAtlasWriteImage(const PathString& a_path, const FontAtlas& a_atlas);

    bool RenderText(FontAtlas& a_font_atlas, const RCommandList a_list, GPUUploadRingAllocator& a_ring_buffer, const GPUFenceValue a_fence_value, const uint2 a_draw_area, const RImageView a_render_target, GPULinearBuffer& a_frame_buffer, const float2 a_text_size, const float2 a_text_start_pos, const StringView a_string);
}
