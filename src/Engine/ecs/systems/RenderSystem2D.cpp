#include "RenderSystem2D.hpp"
#include "Rendererfwd.hpp"
#include "Renderer.hpp"
#include "MaterialSystem.hpp"

#include "AssetLoader.hpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

using namespace BB;

FontAtlas BB::CreateFontAtlas(MemoryArena& a_arena, const PathString& a_font_path, const float a_pixel_height, const int a_first_char)
{
    const Buffer file = OSReadFile(a_arena, a_font_path.c_str());
    const unsigned char* file_c = reinterpret_cast<const unsigned char*>(file.data);
    stbtt_fontinfo font;
    stbtt_InitFont(&font, file_c, stbtt_GetFontOffsetForIndex(file_c, 0));

    const float scale = stbtt_ScaleForPixelHeight(&font, a_pixel_height);
    int ascent;
    stbtt_GetFontVMetrics(&font, &ascent, nullptr, nullptr);

    int max_width = 0; 
    int max_height = 0;
    int total_area = 0;

    for (int i = 0; i < font.numGlyphs; i++)
    {
        const int char_code = a_first_char + i;
        int x0, y0, x1, y1;
        stbtt_GetCodepointBitmapBox(&font, char_code, scale, scale, &x0, &y0, &x1, &y1);

        const int width = x1 - x0;
        const int height = y1 - y0;

        if (width > max_width) max_width = width;
        if (height > max_height) max_height = height;
        total_area += width * height;
    }

    int atlas_size = 1;
    while (atlas_size * atlas_size < total_area * 1.5) 
        atlas_size *= 2;

    if (atlas_size < max_width) atlas_size = max_width;
    if (atlas_size < max_height) atlas_size = max_height;

    FontAtlas atl;
    atl.char_start = a_first_char;
    atl.char_count = font.numGlyphs;
    atl.glyphs = ArenaAllocArr(a_arena, Glyph, font.numGlyphs);
    atl.bitmap = ArenaAllocArr(a_arena, unsigned char, atlas_size * atlas_size);
    atl.extent = uint2(static_cast<uint32_t>(atlas_size), static_cast<uint32_t>(atlas_size));
    atl.pixel_height = a_pixel_height;

    int current_x = 0;
    int current_y = 0;
    int row_height = 0;

    for (int i = 0; i < font.numGlyphs; i++)
    {
        const int char_code = a_first_char + i;
        int advance, lsb, x0, y0, x1, y1;
        stbtt_GetCodepointHMetrics(&font, char_code, &advance, &lsb);
        stbtt_GetCodepointBitmapBox(&font, char_code, scale, scale, &x0, &y0, &x1, &y1);
        const int width = x1 - x0;
        const int height = y1 - y0;

        const int height_mod = max_height - height;
        const int y_offset = height_mod / 2;

        // Check if we need to move to next row
        if (current_x + width > atlas_size) {
            current_x = 0;
            current_y += row_height;
            row_height = 0;
        }

        BB_ASSERT(current_y + height <= atlas_size, "Font atlas too small");

        atl.glyphs[i].pos.x = current_x;
        atl.glyphs[i].pos.y = current_y;
        atl.glyphs[i].extent.x = width;
        atl.glyphs[i].extent.y = height;
        atl.glyphs[i].y_offset = y_offset;
        atl.glyphs[i].advance = advance * scale;

        if (width > 0 && height > 0)
        {
            const int glyph_index = stbtt_FindGlyphIndex(&font, char_code);
            const int bitmap_offset = current_y * atlas_size + current_x;
            stbtt_MakeGlyphBitmap(&font, atl.bitmap + bitmap_offset, width, height, atlas_size, scale, scale, glyph_index);
        }

        current_x += width;
        if (height > row_height)
            row_height = height;
    }

    return atl;
}

bool BB::FontAtlasWriteImage(const PathString& a_path, const FontAtlas& a_atlas)
{
    return Asset::WriteImage(a_path.GetView(), a_atlas.extent, 1, a_atlas.bitmap);
}

bool RenderSystem2D::Init(MemoryArena& a_arena, const PathString& a_font_path)
{
    FixedArray<MaterialShaderCreateInfo, 2> m_shaders;
    m_shaders[0].path = "hlsl/text2d.hlsl";
    m_shaders[0].entry = "VertexMain";
    m_shaders[0].stage = SHADER_STAGE::VERTEX;
    m_shaders[0].next_stages = static_cast<uint32_t>(SHADER_STAGE::FRAGMENT_PIXEL);
    m_shaders[1].path = "hlsl/text2d.hlsl";
    m_shaders[1].entry = "FragmentMain";
    m_shaders[1].stage = SHADER_STAGE::FRAGMENT_PIXEL;
    m_shaders[1].next_stages = static_cast<uint32_t>(SHADER_STAGE::NONE);

    MaterialCreateInfo material{};
    material.pass_type = PASS_TYPE::SCENE;
    material.material_type = MATERIAL_TYPE::MATERIAL_2D;
    material.cpu_writeable = false;
    material.shader_infos = m_shaders.slice();
    MemoryArenaScope(a_arena)
    {
        m_material = Material::CreateMasterMaterial(a_arena, material, a_font_path.GetView(a_font_path.size() - a_font_path.find_last_of_directory_slash(), a_font_path.find_last_of_directory_slash()));
    }
    return true;
}

void RenderSystem2D::Destroy()
{
    Asset::FreeAsset(m_image_asset);
}

void RenderSystem2D::RenderText(const RCommandList a_list, GPUUploadRingAllocator& a_ring_buffer, const GPULinearBuffer& a_frame_buffer, const uint2 a_text_size, const uint2 a_text_start_pos, const StringView a_string)
{
    
}
