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
        int char_v;
    };

    struct FontAtlas
    {
        uint2 extent;
        unsigned char* bitmap;
        int char_start;
        int char_count;
        Glyph* glyphs;
        float pixel_height;
    };

    FontAtlas CreateFontAtlas(MemoryArena& a_arena, const PathString& a_font_path, const float a_pixel_height, const int a_first_char);

    class RenderSystem2D
    {
    public:
        bool Init(MemoryArena& a_arena, const PathString& a_font_path);
        void Destroy();

        void RenderText(const RCommandList a_list, GPUUploadRingAllocator& a_ring_buffer, const GPULinearBuffer& a_frame_buffer, const uint2 a_text_size, const uint2 a_text_start_pos, const StringView a_string);

    private:
        uint2 m_image_size;
        RDescriptorIndex m_font_atlas;
        AssetHandle m_image_asset;

        StaticArray<Glyph2D> m_glyps;
        StaticArray<FixedArray<float2, 2>> m_uvs;

        MasterMaterialHandle m_material;
    };
}
