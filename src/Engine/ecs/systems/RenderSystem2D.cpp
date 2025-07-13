#include "RenderSystem2D.hpp"
#include "Rendererfwd.hpp"
#include "Renderer.hpp"
#include "MaterialSystem.hpp"

#include "AssetLoader.hpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

using namespace BB;

using I_TYPE = uint32_t;

static size_t GetIFromXY(int a_x, int a_y, int a_max_x)
{
    return static_cast<size_t>(a_x + a_y * a_max_x);
}

static ConstSlice<uint32_t> GetVerticalColumns(MemoryArena& a_arena, const I_TYPE a_background, const I_TYPE* a_pixels, const int a_width, const int a_height)
{
    uint32_t current_columns = 1;
    uint32_t column_count = 8;
    uint32_t* columns = ArenaAllocArr(a_arena, uint32_t, column_count);
    columns[0] = 0;

    for (int x = 0; x < a_width; x++)
    {
        bool empty_column = true;

        for (size_t y = 0; y < a_height; y++)
        {
            if (a_pixels[GetIFromXY(x, y, a_width)] != a_background)
            {
                empty_column = false;
                break;
            }
        }

        if (empty_column)
        {
            if (current_columns == 0 || x > columns[current_columns - 1] + 1)
            {
                if (current_columns + 1 == column_count)
                {
                    const uint32_t new_column_count = column_count * 2;
                    columns = reinterpret_cast<uint32_t*>(ArenaRealloc(a_arena, columns, sizeof(uint32_t) * column_count, sizeof(uint32_t) * new_column_count, alignof(uint32_t)));
                    column_count = new_column_count;
                }
                columns[current_columns++] = x;
            }
        }
    }

    return ConstSlice(columns, current_columns);
}

static StaticArray<FixedArray<float2, 2>> CreateUVGlyphsFromImage(MemoryArena& a_arena, const I_TYPE* a_pixels, const int a_width, const int a_height, const int a_bytes_per_pixel)
{
    int cur_x = 0, cur_y = 0;
    const I_TYPE background = a_pixels[0];

    const ConstSlice<uint32_t> columns = GetVerticalColumns(a_arena, background, a_pixels, a_width, a_height);
}

// old code maybe one day I'll use it
static void FontFromImage()
{
    //// create material here
    //int width, height, bytes_per_pixel;
    //unsigned char* pixels = Asset::LoadImageCPU(a_font_path.c_str(), width, height, bytes_per_pixel);

    //m_image_size = uint2(static_cast<uint32_t>(width), static_cast<uint32_t>(height));

    //Asset::TextureLoadFromMemory texture_load;
    //texture_load.name = "font image";
    //texture_load.width = m_image_size.x;
    //texture_load.height = m_image_size.y;
    //texture_load.pixels = pixels;
    //texture_load.bytes_per_pixel = static_cast<uint32_t>(bytes_per_pixel);
    //MemoryArenaScope(a_arena)
    //{
    //    const Image& img = Asset::LoadImageMemory(a_arena, texture_load);
    //    m_image_asset = img.asset_handle;
    //    m_font_atlas = img.descriptor_index;
    //}


    //m_uvs = CreateUVGlyphsFromImage(a_arena, pixels, width, height, bytes_per_pixel);
    //m_glyps.Init(a_arena, a_max_glyphs_per_frame);


    //Asset::FreeImageCPU(pixels);
}

FontAtlas BB::CreateFontAtlas(MemoryArena& a_arena, const PathString& a_font_path, const float a_pixel_height, const int a_first_char)
{
    const Buffer file = OSReadFile(a_arena, a_font_path.c_str());
    const unsigned char* file_c = reinterpret_cast<const unsigned char*>(file.data);
    stbtt_fontinfo font;
    stbtt_InitFont(&font, file_c, stbtt_GetFontOffsetForIndex(file_c, 0));

    const float scale = stbtt_ScaleForPixelHeight(&font, a_pixel_height);

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
        int x0, y0, x1, y1;
        stbtt_GetCodepointBitmapBox(&font, char_code, scale, scale, &x0, &y0, &x1, &y1);

        const int width = x1 - x0;
        const int height = y1 - y0;

        // Check if we need to move to next row
        if (current_x + width > atlas_size) {
            current_x = 0;
            current_y += row_height;
            row_height = 0;
        }

        BB_ASSERT(current_y + height <= atlas_size, "Font atlas too small");

        int advance, lsb;
        stbtt_GetCodepointHMetrics(&font, char_code, &advance, &lsb);

        atl.glyphs[i].pos.x = current_x;
        atl.glyphs[i].pos.y = current_y;
        atl.glyphs[i].extent.x = width;
        atl.glyphs[i].extent.y = height;
        atl.glyphs[i].char_v = char_code;
        atl.glyphs[i].advance = advance * scale;

        if (width > 0 && height > 0)
        {
            const int glyph_index = stbtt_FindGlyphIndex(&font, char_code);
            unsigned char* bitmap_offset = atl.bitmap + current_y * atlas_size + current_x;
            stbtt_MakeGlyphBitmap(&font, bitmap_offset, width, height, atlas_size, scale, scale, glyph_index);
        }

        current_x += width;
        if (height > row_height)
            row_height = height;
    }

    return atl;
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
