#include "UICanvas.hpp"
#include "Rendererfwd.hpp"
#include "Renderer.hpp"
#include "MaterialSystem.hpp"

#include "AssetLoader.hpp"

#include "Math/Math.inl"

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
    int ascent, descent, gap;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &gap);
    const int baseline = static_cast<int>(static_cast<float>(ascent) * scale);

    int max_width = 0; 
    int max_height = 0;
    int total_area = 0;

    for (int i = 0; i < font.numGlyphs; i++)
    {
        const int char_code = a_first_char + i;
        int x0, y0, x1, y1;
        stbtt_GetCodepointBitmapBox(&font, char_code, scale, scale, &x0, &y0, &x1, &y1);

        const int width = x1 - x0;
        const int height = baseline + y1;

        if (width > max_width) max_width = width;
        if (height > max_height) max_height = height;
        total_area += width * height;
    }

    int atlas_size = 1;
    while (atlas_size * atlas_size < total_area * 1.5) 
        atlas_size *= 2;

    if (atlas_size < max_width) atlas_size = max_width;
    if (atlas_size < max_height) atlas_size = max_height;

    const uint2 extent = uint2(static_cast<uint32_t>(atlas_size), static_cast<uint32_t>(atlas_size));

    const uint32_t bitmap_size = static_cast<uint32_t>(atlas_size * atlas_size);
    FontAtlas atl;
    atl.extent = extent;
    atl.char_start = a_first_char;
    atl.char_count = font.numGlyphs;
    atl.glyphs = ArenaAllocArr(a_arena, Glyph, static_cast<uint32_t>(font.numGlyphs));
    atl.pixel_height = a_pixel_height;
    atl.text_height = ceilf(static_cast<float>(ascent - descent + gap) * scale);

    int current_x = 0;
    int current_y = 0;
    int row_height = 0;

    MemoryArenaScope(a_arena)
    {
        unsigned char* bitmap = ArenaAllocArr(a_arena, unsigned char, bitmap_size);
        for (int i = 0; i < font.numGlyphs; i++)
        {
            const int char_code = a_first_char + i;
            int advance, lsb, x0, y0, x1, y1;
            stbtt_GetCodepointHMetrics(&font, char_code, &advance, &lsb);
            stbtt_GetCodepointBitmapBox(&font, char_code, scale, scale, &x0, &y0, &x1, &y1);
            const int width = x1 - x0;
            const int height = baseline + y1;

            // Check if we need to move to next row
            if (current_x + width > atlas_size) 
            {
                current_x = 0;
                current_y += row_height;
                row_height = 0;
            }
            const int advance_scaled = static_cast<int>(static_cast<float>(advance) * scale);
            atl.glyphs[i].pos.x = current_x;
            atl.glyphs[i].pos.y = current_y;
            atl.glyphs[i].extent.x = width;
            atl.glyphs[i].extent.y = height;
            atl.glyphs[i].advance = advance_scaled;

            if (width > 0 && height > 0)
            {
                const int glyph_index = stbtt_FindGlyphIndex(&font, char_code);
                const int bitmap_offset = (current_y + baseline + y0) * atlas_size + current_x;
                stbtt_MakeGlyphBitmap(&font, bitmap + bitmap_offset, width, height, atlas_size, scale, scale, glyph_index);
            }

            current_x += advance_scaled;
            if (height > row_height)
                row_height = height;
        }

        Asset::TextureLoadFromMemory load_info;
        load_info.name = "font";
        load_info.bytes_per_pixel = 1;
        load_info.width = extent.x;
        load_info.height = extent.y;
        load_info.pixels = bitmap;
        const Image& img = Asset::LoadImageMemory(a_arena, load_info);
        atl.image = img.gpu_image;
        atl.image_index = img.descriptor_index;
        atl.asset = img.asset_handle;
    }

    return atl;
}

void UICanvas::BeginDraw(MemoryArena& a_arena, const uint32_t a_max_quads)
{
    m_quads.Init(a_arena, a_max_quads);
}

void UICanvas::CreatePanel(const float2 a_pos, const float2 a_extent, const Color a_color)
{
    Quad2D quad;
    quad.pos = a_pos;
    quad.extent = a_extent;
    quad.uv0 = float2(0.f);
    quad.uv1 = float2(1.f);
    quad.color = a_color;
    quad.text_id = Asset::GetWhiteTexture();
    m_quads.emplace_back(quad);
}

bool UICanvas::CreateText(const float2 a_pos, const float2 a_extent, const Color a_color, const StringView a_string, const float a_x_length, const float a_spacing, const FontAtlas& a_font)
{
    float2 pos = a_pos;

    for (size_t i = 0; i < a_string.size(); i++)
    {
        const char ch = a_string[i];

        if (ch == '\n')
        {
            const float spacing = a_font.text_height * a_extent.y + a_spacing;
            pos.x = a_pos.x;
            pos.y += spacing;
        }
        else
        {
            const int char_index = ch - a_font.char_start;
            if (char_index > a_font.char_count)
            {
                BB_WARNING(false, "trying to write a char that doesn't exist or goes out of bounds", WarningType::MEDIUM);
                return false;
            }
            const Glyph rd_gly = a_font.glyphs[char_index];
            const float2 tex_pos = float2(static_cast<float>(rd_gly.pos.x), static_cast<float>(rd_gly.pos.y));
            const float2 rd_extent = float2(static_cast<float>(rd_gly.extent.x), static_cast<float>(rd_gly.extent.y));
            const float2 wr_extent = rd_extent * a_extent;

            if (pos.x + wr_extent.x > a_pos.x + a_x_length)
            {
                const float spacing = a_font.text_height * a_extent.y + a_spacing;
                pos.x = a_pos.x;
                pos.y += spacing;
            }

            Quad2D quad;
            quad.pos = float2(pos.x, pos.y);
            quad.extent = wr_extent;
            quad.uv0 = tex_pos / float2(static_cast<float>(a_font.extent.x), static_cast<float>(a_font.extent.y));
            quad.uv1 = (tex_pos + rd_extent) / float2(static_cast<float>(a_font.extent.x), static_cast<float>(a_font.extent.y));
            quad.color = a_color;
            quad.text_id = a_font.image_index;
            m_quads.emplace_back(quad);
            pos.x += static_cast<float>(rd_gly.advance) * a_extent.x;
        }
    }
    return true;
}

bool UICanvas::EndDraw(const RCommandList a_list, const GPUFenceValue a_fence_value, GPUUploadRingAllocator& a_ring_buffer, GPULinearBuffer& a_frame_buffer, const uint2 a_draw_area, const RImageView a_render_target, const MasterMaterialHandle a_material) const
{
    if (m_quads.IsEmpty())
        return false;
    const size_t memsize = m_quads.size() * sizeof(Quad2D);

    const uint64_t src_offset = a_ring_buffer.AllocateUploadMemory(memsize, a_fence_value);
    if (src_offset == uint64_t(-1))
    {
        BB_WARNING(false, "out of upload buffer space for UICanvas", WarningType::HIGH);
        return false;
    }
    GPUBufferView dst;
    if (!a_frame_buffer.Allocate(memsize, dst))
    {
        BB_WARNING(false, "out of per frame buffer space for UICanvas", WarningType::HIGH);
        return false;
    }

    a_ring_buffer.MemcpyIntoBuffer(src_offset, m_quads.data(), memsize);

    RenderCopyBufferRegion region;
    region.size = memsize;
    region.src_offset = src_offset;
    region.dst_offset = dst.offset;
    RenderCopyBuffer copy;
    copy.src = a_ring_buffer.GetBuffer();
    copy.dst = a_frame_buffer.GetBuffer();
    copy.regions = Slice(&region, 1);
    CopyBuffer(a_list, copy);

    RenderingAttachmentColor color_attach;
    color_attach.load_color = true;
    color_attach.store_color = true;
    color_attach.image_layout = IMAGE_LAYOUT::RT_COLOR;
    color_attach.image_view = a_render_target;

    StartRenderingInfo start_rendering_info;
    start_rendering_info.render_area_extent = a_draw_area;
    start_rendering_info.render_area_offset = int2{ 0, 0 };
    start_rendering_info.layer_count = 1;
    start_rendering_info.view_mask = 0;
    start_rendering_info.color_attachments = Slice(&color_attach, 1);
    start_rendering_info.depth_attachment = nullptr;

    FixedArray<ColorBlendState, 1> blend_state;
    blend_state[0].blend_enable = true;
    blend_state[0].color_flags = 0xF;
    blend_state[0].color_blend_op = BLEND_OP::ADD;
    blend_state[0].src_blend = BLEND_MODE::FACTOR_SRC_ALPHA;
    blend_state[0].dst_blend = BLEND_MODE::FACTOR_ONE_MINUS_SRC_ALPHA;
    blend_state[0].alpha_blend_op = BLEND_OP::ADD;
    blend_state[0].src_alpha_blend = BLEND_MODE::FACTOR_ONE;
    blend_state[0].dst_alpha_blend = BLEND_MODE::FACTOR_ONE_MINUS_SRC_ALPHA;
    SetBlendMode(a_list, 0, blend_state.slice());
    StartRenderPass(a_list, start_rendering_info);

    SetPrimitiveTopology(a_list, PRIMITIVE_TOPOLOGY::TRIANGLE_LIST);
    Material::BindMaterial(a_list, a_material);

    ShaderIndices2DQuads push_constant;
    push_constant.per_frame_buffer_start = static_cast<uint32_t>(dst.offset);

    SetPushConstantUserData(a_list, 0, sizeof(push_constant), &push_constant);

    DrawVertices(a_list, 6, m_quads.size(), 1, 0);

    EndRenderPass(a_list);
    return true;
}

void UICanvas::Clear()
{
    m_quads.clear();
}
