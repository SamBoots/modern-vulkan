#include "RenderSystem2D.hpp"
#include "Rendererfwd.hpp"
#include "Renderer.hpp"
#include "MaterialSystem.hpp"

#include "AssetLoader.hpp"

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

bool RenderSystem2D::Init(MemoryArena& a_arena, const size_t a_max_glyphs_per_frame, const PathString& a_font_path)
{
    // create material here
    int width, height, bytes_per_pixel;
    unsigned char* pixels = Asset::LoadImageCPU(a_font_path.c_str(), width, height, bytes_per_pixel);

    m_image_size = uint2(static_cast<uint32_t>(width), static_cast<uint32_t>(height));

    Asset::TextureLoadFromMemory texture_load;
    texture_load.name = "font image";
    texture_load.width = m_image_size.x;
    texture_load.height = m_image_size.y;
    texture_load.pixels = pixels;
    texture_load.bytes_per_pixel = static_cast<uint32_t>(bytes_per_pixel);
    MemoryArenaScope(a_arena)
    {
        const Image& img = Asset::LoadImageMemory(a_arena, texture_load);
        m_image_asset = img.asset_handle;
        m_font_atlas = img.descriptor_index;
    }

    m_uvs = CreateUVGlyphsFromImage(a_arena, pixels, width, height, bytes_per_pixel);
    m_glyps.Init(a_arena, a_max_glyphs_per_frame);


    Asset::FreeImageCPU(pixels);

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
