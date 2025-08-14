#include "RasterMeshStage.hpp"
#include "Renderer.hpp"
#include "MaterialSystem.hpp"

using namespace BB;

void RasterMeshStage::Init(MemoryArena& a_arena, const uint2 a_render_target_extent, const uint32_t a_back_buffer_count)
{
    m_per_frame.Init(a_arena, a_back_buffer_count);
    m_per_frame.resize(a_back_buffer_count);
    for (uint32_t i = 0; i < m_per_frame.size(); i++)
    {
        PerFrame& pfd = m_per_frame[i];
        CreateDepthImages(pfd, a_render_target_extent);
    }
}

void RasterMeshStage::ExecutePass(const RCommandList a_list, const uint32_t a_frame_index, const uint2 a_draw_area_size, const DrawList& a_draw_list, const RImageView a_render_target, const RImageView a_render_target_bright)
{
    PerFrame& pfd = m_per_frame[a_frame_index];

    if (pfd.depth_extent != a_draw_area_size)
    {
        FreeImage(pfd.depth_image);
        FreeImageViewShaderInaccessible(pfd.depth_image_view);
        CreateDepthImages(pfd, a_draw_area_size);
    }

    PipelineBarrierImageInfo image_transitions[1]{};
    image_transitions[0].prev = IMAGE_LAYOUT::NONE;
    image_transitions[0].next = IMAGE_LAYOUT::RT_DEPTH;
    image_transitions[0].image = pfd.depth_image;
    image_transitions[0].layer_count = 1;
    image_transitions[0].level_count = 1;
    image_transitions[0].base_array_layer = 0;
    image_transitions[0].base_mip_level = 0;
    image_transitions[0].image_aspect = IMAGE_ASPECT::DEPTH_STENCIL;

    PipelineBarrierInfo pipeline_info = {};
    pipeline_info.image_barriers = ConstSlice<PipelineBarrierImageInfo>(image_transitions, 1);
    PipelineBarriers(a_list, pipeline_info);

    FixedArray<RenderingAttachmentColor, 2> color_attachs;
    color_attachs[0].load_color = true;
    color_attachs[0].store_color = true;
    color_attachs[0].image_layout = IMAGE_LAYOUT::RT_COLOR;
    color_attachs[0].image_view = a_render_target;

    color_attachs[1].load_color = false;
    color_attachs[1].store_color = true;
    color_attachs[1].image_layout = IMAGE_LAYOUT::RT_COLOR;
    color_attachs[1].image_view = a_render_target_bright;
    const uint32_t color_attach_count = 2;

    RenderingAttachmentDepth depth_attach{};
    depth_attach.load_depth = false;
    depth_attach.store_depth = true;
    depth_attach.image_layout = IMAGE_LAYOUT::RT_DEPTH;
    depth_attach.image_view = pfd.depth_image_view;

    StartRenderingInfo rendering_info;
    rendering_info.color_attachments = color_attachs.slice(color_attach_count);
    rendering_info.depth_attachment = &depth_attach;
    rendering_info.render_area_extent = a_draw_area_size;
    rendering_info.render_area_offset = int2{ 0, 0 };

    StartRenderPass(a_list, rendering_info);
    SetDepthBias(a_list, 0.f, 0.f, 0.f);
    FixedArray<ColorBlendState, 2> blend_state;
    blend_state[0].blend_enable = true;
    blend_state[0].color_flags = 0xF;
    blend_state[0].color_blend_op = BLEND_OP::ADD;
    blend_state[0].src_blend = BLEND_MODE::FACTOR_SRC_ALPHA;
    blend_state[0].dst_blend = BLEND_MODE::FACTOR_ONE_MINUS_SRC_ALPHA;
    blend_state[0].alpha_blend_op = BLEND_OP::ADD;
    blend_state[0].src_alpha_blend = BLEND_MODE::FACTOR_ONE;
    blend_state[0].dst_alpha_blend = BLEND_MODE::FACTOR_ZERO;
    blend_state[1] = blend_state[0];
    SetBlendMode(a_list, 0, blend_state.slice(color_attach_count));

    //if (m_options.skip_object_rendering)
    //{
    //    EndRenderPass(a_list);
    //    return;
    //}
    SetFrontFace(a_list, false);
    SetCullMode(a_list, CULL_MODE::NONE);

    for (uint32_t i = 0; i < a_draw_list.draw_entries.size(); i++)
    {
        const DrawList::DrawEntry& mesh_draw_call = a_draw_list.draw_entries[i];

        SetPrimitiveTopology(a_list, PRIMITIVE_TOPOLOGY::TRIANGLE_LIST);
        Material::BindMaterial(a_list, mesh_draw_call.master_material);

        const size_t position_byte_size = mesh_draw_call.mesh.vertex_normal_offset - mesh_draw_call.mesh.vertex_position_offset;
        const uint32_t vertex_count = static_cast<uint32_t>(position_byte_size / sizeof(float3));

        PBRIndices shader_indices;
        shader_indices.transform_index = i;
        shader_indices.vertex_offset = static_cast<uint32_t>(mesh_draw_call.mesh.vertex_position_offset);
        shader_indices.vertex_count = vertex_count;
        shader_indices.material_index = RDescriptorIndex(mesh_draw_call.material.index);
        SetPushConstantUserData(a_list, 0, sizeof(shader_indices), &shader_indices);

        DrawIndexed(a_list,
            mesh_draw_call.index_count,
            1,
            static_cast<uint32_t>(mesh_draw_call.mesh.index_buffer_offset / sizeof(uint32_t)) + mesh_draw_call.index_start,
            0,
            0);
    }

    EndRenderPass(a_list);
}

void RasterMeshStage::CreateDepthImages(PerFrame& a_frame, const uint2 a_extent)
{
    ImageCreateInfo depth_img_info;
    depth_img_info.name = "scene depth buffer";
    depth_img_info.width = a_extent.x;
    depth_img_info.height = a_extent.y;
    depth_img_info.depth = 1;
    depth_img_info.mip_levels = 1;
    depth_img_info.array_layers = 1;
    depth_img_info.format = IMAGE_FORMAT::D24_UNORM_S8_UINT;
    depth_img_info.usage = IMAGE_USAGE::DEPTH;
    depth_img_info.type = IMAGE_TYPE::TYPE_2D;
    depth_img_info.use_optimal_tiling = true;
    depth_img_info.is_cube_map = false;
    a_frame.depth_image = CreateImage(depth_img_info);

    ImageViewCreateInfo depth_img_view_info;
    depth_img_view_info.name = "scene depth view";
    depth_img_view_info.image = a_frame.depth_image;
    depth_img_view_info.type = IMAGE_VIEW_TYPE::TYPE_2D;
    depth_img_view_info.base_array_layer = 0;
    depth_img_view_info.array_layers = 1;
    depth_img_view_info.mip_levels = 1;
    depth_img_view_info.base_mip_level = 0;
    depth_img_view_info.format = IMAGE_FORMAT::D24_UNORM_S8_UINT;
    depth_img_view_info.aspects = IMAGE_ASPECT::DEPTH_STENCIL;
    a_frame.depth_image_view = CreateImageViewShaderInaccessible(depth_img_view_info);
    a_frame.depth_extent = a_extent;
}
