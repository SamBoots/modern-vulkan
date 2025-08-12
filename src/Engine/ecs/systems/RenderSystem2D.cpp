#include "RenderSystem2D.hpp"
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

bool BB::RenderText(const FontAtlas& a_font_atlas, const RCommandList a_list, GPUUploadRingAllocator& a_ring_buffer, GPULinearBuffer& a_frame_buffer, const float2 a_text_size, const float2 a_text_start_pos, const StringView a_string)
{

    /*const size_t upload_size = a_string.size() * sizeof(Glyph2D);
    const size_t upload_start = a_ring_buffer.AllocateUploadMemory(upload_size, ...);
    if (upload_start == -1)
        return false;

    GPUBufferView buffer_view;
    bool success = a_frame_buffer.Allocate(upload_size, buffer_view);
    if (!success)
        return false;

    float2 pos = a_text_start_pos;

    for (size_t i = 0; i < a_string.size(); i++)
    {
        const size_t char_index = a_string[i] - a_font_atlas.char_start;
        if (char_index > a_font_atlas.char_count)
        {
            BB_WARNING(false, "trying to write a char that doesn't exist or goes out of bounds", WarningType::MEDIUM);
            return false;
        }
        const Glyph rd_gly = a_font_atlas.glyphs[char_index];

        Glyph2D wr_gly;
        wr_gly.pos = pos;
        wr_gly.extent.x = static_cast<float>(rd_gly.extent.x + a_text_size.x);
        wr_gly.extent.y = static_cast<float>(rd_gly.extent.y + a_text_size.y);
        wr_gly.uv0.x = static_cast<float>(rd_gly.pos.x / a_font_atlas.extent.x);
        wr_gly.uv0.y = static_cast<float>(rd_gly.pos.y / a_font_atlas.extent.y);
        wr_gly.uv1.x = static_cast<float>((rd_gly.pos.x + rd_gly.extent.x) / a_font_atlas.extent.x);
        wr_gly.uv1.y = static_cast<float>((rd_gly.pos.y + rd_gly.extent.y) / a_font_atlas.extent.y);
        a_ring_buffer.MemcpyIntoBuffer(upload_start + i * sizeof(Glyph2D), &wr_gly, sizeof(wr_gly));

        pos.x += rd_gly.advance;
    }

    RenderCopyBufferRegion region;
    region.size = upload_size;
    region.src_offset = upload_start;
    region.dst_offset = buffer_view.offset;
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
    blend_state[0].dst_alpha_blend = BLEND_MODE::FACTOR_ZERO;
    SetBlendMode(a_list, 0, blend_state.slice());
    StartRenderPass(a_list, start_rendering_info);

    SetPrimitiveTopology(a_list, PRIMITIVE_TOPOLOGY::LINE_LIST);
    const RPipelineLayout pipe_layout = Material::BindMaterial(a_list, m_line_material);

    ShaderIndicesGlyph push_constant;
    push_constant.glyph_buffer_offset = buffer_view.offset;
    push_constant.font_texture = a_font_atlas.desc_index;

    SetPushConstants(a_list, pipe_layout, 0, sizeof(push_constant), &push_constant);

    DrawVertices(a_list, 4, static_cast<uint32_t>(a_string.size()), 1, 0);
    
    EndRenderPass(a_list);*/

    return true;
}
