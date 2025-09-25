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
    };

    FontAtlas CreateFontAtlas(MemoryArena& a_arena, const PathString& a_font_path, const float a_pixel_height, const int a_first_char);

    class UICanvas
    {
    public:
        void BeginDraw(MemoryArena& a_arena, const uint32_t a_max_quads);
        void CreatePanel(const float2 a_pos, const float2 a_extent, const Color a_color);
        bool CreateText(const float2 a_pos, const float2 a_extent, const Color a_color, const StringView a_string, const float a_x_length, const float a_spacing, const FontAtlas& a_font);
        bool EndDraw(const RCommandList a_list, const GPUFenceValue a_fence_value, GPUUploadRingAllocator& a_ring_buffer, GPULinearBuffer& a_frame_buffer, const uint2 a_draw_area, const RImageView a_render_target, const MasterMaterialHandle a_material) const;
        ConstSlice<Quad2D> GetQuads() const { return m_quads.const_slice(); }
        void Clear();
    private:
        StaticArray<Quad2D> m_quads;
    };
}
