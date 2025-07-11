#include "LineStage.hpp"
#include "Renderer.hpp"
#include "MaterialSystem.hpp"

using namespace BB;

void LineStage::Init(MemoryArena& a_arena, const uint32_t a_back_buffer_count, const uint32_t a_line_max)
{
    MaterialCreateInfo line_material;
    line_material.pass_type = PASS_TYPE::SCENE;
    line_material.material_type = MATERIAL_TYPE::NONE;
    FixedArray<MaterialShaderCreateInfo, 3> line_shaders;
    line_shaders[0].path = "hlsl/Line.hlsl";
    line_shaders[0].entry = "VertexMain";
    line_shaders[0].stage = SHADER_STAGE::VERTEX;
    line_shaders[0].next_stages = static_cast<uint32_t>(SHADER_STAGE::GEOMETRY);
    line_shaders[1].path = "hlsl/Line.hlsl";
    line_shaders[1].entry = "GeometryMain";
    line_shaders[1].stage = SHADER_STAGE::GEOMETRY;
    line_shaders[1].next_stages = static_cast<uint32_t>(SHADER_STAGE::FRAGMENT_PIXEL);
    line_shaders[2].path = "hlsl/Line.hlsl";
    line_shaders[2].entry = "FragmentMain";
    line_shaders[2].stage = SHADER_STAGE::FRAGMENT_PIXEL;
    line_shaders[2].next_stages = static_cast<uint32_t>(SHADER_STAGE::NONE);
    line_material.shader_infos = Slice(line_shaders.slice());
    MemoryArenaScope(a_arena)
    {
        m_line_material = Material::CreateMasterMaterial(a_arena, line_material, "line material");
    }

    m_per_frame.Init(a_arena, a_back_buffer_count);
    m_per_frame.resize(a_back_buffer_count);
    for (uint32_t i = 0; i < m_per_frame.size(); i++)
    {
        PerFrame& pfd = m_per_frame[i];
        pfd.vertex_view = AllocateFromWritableVertexBuffer(a_line_max * sizeof(Line));
    }
}

void LineStage::ExecutePass(const RCommandList a_list, const uint32_t a_frame_index, const uint2 a_draw_area, const RImageView a_render_target, const RImageView a_depth_buffer)
{
    PerFrame& pfd = m_per_frame[a_frame_index];
    if (pfd.lines == 0)
        return;

    RenderingAttachmentColor color_attach;
    color_attach.load_color = true;
    color_attach.store_color = true;
    color_attach.image_layout = IMAGE_LAYOUT::RT_COLOR;
    color_attach.image_view = a_render_target;

    RenderingAttachmentDepth* pdepth = nullptr;
    RenderingAttachmentDepth depth;
    if (a_depth_buffer.IsValid())
    {
        depth = {};
        depth.load_depth = true;
        depth.store_depth = false;
        depth.image_layout = IMAGE_LAYOUT::RT_DEPTH;
        depth.image_view = a_depth_buffer;
        pdepth = &depth;
    }

    StartRenderingInfo start_rendering_info;
    start_rendering_info.render_area_extent = a_draw_area;
    start_rendering_info.render_area_offset = int2{ 0, 0 };
    start_rendering_info.color_attachments = Slice(&color_attach, 1);
    start_rendering_info.depth_attachment = pdepth;

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

    ShaderLine push_constant;
    push_constant.line_width = 1.5f;
    push_constant.vertex_start = static_cast<uint32_t>(pfd.vertex_view.offset);

    SetPushConstants(a_list, pipe_layout, 0, sizeof(push_constant), &push_constant);

    DrawVertices(a_list, pfd.lines * 2, 1, 0, 0);

    EndRenderPass(a_list);
    pfd.lines = 0;
}

void LineStage::AddLines(const uint32_t a_frame_index, const ConstSlice<Line> a_lines)
{
    PerFrame& pfd = m_per_frame[a_frame_index];

    size_t upload_size = a_lines.sizeInBytes();
    if (upload_size > pfd.vertex_view.size)
    {
        upload_size = pfd.vertex_view.size;
        BB_WARNING(false, "uploading too many lines, not all may be shown", WarningType::LOW);
    }
    void* mapped_offset = Pointer::Add(pfd.vertex_view.mapped, sizeof(Line) * pfd.lines);
    memcpy(mapped_offset, a_lines.data(), upload_size);
    pfd.lines += static_cast<uint32_t>(a_lines.size());
}
