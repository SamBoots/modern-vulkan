#include "RenderStages.hpp"
#include "Renderer.hpp"
#include "MaterialSystem.hpp"

using namespace BB;

bool BB::RenderPassClearStage(RG::RenderGraph& a_graph, const RCommandList a_list, const MasterMaterialHandle a_material, Slice<RG::ResourceHandle> a_resource_inputs, Slice<RG::ResourceHandle> a_resource_outputs)
{
    const RG::RenderResource& out_rt = a_graph.GetResource(a_resource_outputs[0]);

    SetPrimitiveTopology(a_list, PRIMITIVE_TOPOLOGY::TRIANGLE_LIST);
    Material::BindMaterial(a_list, a_material);

    RenderingAttachmentColor color_attach;
    color_attach.load_color = false;
    color_attach.store_color = true;
    color_attach.image_layout = IMAGE_LAYOUT::RT_COLOR;
    color_attach.image_view = GetImageView(out_rt.descriptor_index);

    StartRenderingInfo start_rendering_info;
    start_rendering_info.render_area_extent = uint2(out_rt.image.extent.x, out_rt.image.extent.y);
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
    blend_state[0].dst_alpha_blend = BLEND_MODE::FACTOR_ZERO;
    SetBlendMode(a_list, 0, blend_state.slice());
    StartRenderPass(a_list, start_rendering_info);

    SetFrontFace(a_list, false);
    SetCullMode(a_list, CULL_MODE::NONE);

    DrawCubemap(a_list, 1, 0);

    EndRenderPass(a_list);
}

bool BB::RenderPassShadowMapStage(RG::RenderGraph& a_graph, const RCommandList a_list, const MasterMaterialHandle a_material, Slice<RG::ResourceHandle> a_resource_inputs, Slice<RG::ResourceHandle> a_resource_outputs)
{

}

bool BB::RenderPassPBRStage(RG::RenderGraph& a_graph, const RCommandList a_list, const MasterMaterialHandle a_material, Slice<RG::ResourceHandle> a_resource_inputs, Slice<RG::ResourceHandle> a_resource_outputs)
{

}

bool BB::RenderPassLineStage(RG::RenderGraph& a_graph, const RCommandList a_list, const MasterMaterialHandle a_material, Slice<RG::ResourceHandle> a_resource_inputs, Slice<RG::ResourceHandle> a_resource_outputs)
{

}

bool BB::RenderPassGlyphStage(RG::RenderGraph& a_graph, const RCommandList a_list, const MasterMaterialHandle a_material, Slice<RG::ResourceHandle> a_resource_inputs, Slice<RG::ResourceHandle> a_resource_outputs)
{

}

bool BB::RenderPassBloomStage(RG::RenderGraph& a_graph, const RCommandList a_list, const MasterMaterialHandle a_material, Slice<RG::ResourceHandle> a_resource_inputs, Slice<RG::ResourceHandle> a_resource_outputs)
{
    const RG::RenderResource& in_rt_0 = a_graph.GetResource(a_resource_inputs[0]);
    const RG::RenderResource& in_rt_1 = a_graph.GetResource(a_resource_inputs[1]);
    const RG::RenderResource& out_rt = a_graph.GetResource(a_resource_outputs[0]);
    const uint2 rt_0_extent = uint2(in_rt_0.image.extent.x, in_rt_0.image.extent.y);
    const uint2 rt_1_extent = uint2(in_rt_1.image.extent.x, in_rt_1.image.extent.y);

    SetPrimitiveTopology(a_list, PRIMITIVE_TOPOLOGY::TRIANGLE_LIST);
    Material::BindMaterial(a_list, a_material);

    FixedArray<PipelineBarrierImageInfo, 2> transitions{};
    PipelineBarrierImageInfo& to_shader_read = transitions[0];
    to_shader_read.prev = IMAGE_LAYOUT::RT_COLOR;
    to_shader_read.next = IMAGE_LAYOUT::RO_FRAGMENT;
    to_shader_read.image = in_rt_0.image.image;
    to_shader_read.layer_count = 1;
    to_shader_read.level_count = 1;
    to_shader_read.base_array_layer = in_rt_0.image.base_array_layer;
    to_shader_read.base_mip_level = 0;
    to_shader_read.image_aspect = IMAGE_ASPECT::COLOR;

    PipelineBarrierImageInfo& to_render_target = transitions[1];
    to_render_target.prev = IMAGE_LAYOUT::RO_FRAGMENT;
    to_render_target.next = IMAGE_LAYOUT::RT_COLOR;
    to_render_target.image = in_rt_0.image.image;
    to_render_target.layer_count = 1;
    to_render_target.level_count = 1;
    to_render_target.base_array_layer = 1;
    to_render_target.base_mip_level = 0;
    to_render_target.image_aspect = IMAGE_ASPECT::COLOR;

    PipelineBarrierInfo barrier_info{};
    barrier_info.image_barriers = transitions.const_slice();

    SetFrontFace(a_list, false);
    SetCullMode(a_list, CULL_MODE::NONE);
    // horizontal bloom slice
    {
        PipelineBarriers(a_list, barrier_info);

        RenderingAttachmentColor color_attach;
        color_attach.load_color = false;
        color_attach.store_color = true;
        color_attach.image_layout = IMAGE_LAYOUT::RT_COLOR;
        color_attach.image_view = GetImageView(in_rt_1.descriptor_index);
        StartRenderingInfo rendering_info;
        rendering_info.color_attachments = Slice(&color_attach, 1);
        rendering_info.depth_attachment = nullptr;
        rendering_info.layer_count = 1;
        rendering_info.view_mask = 0;
        rendering_info.render_area_extent = rt_1_extent;
        rendering_info.render_area_offset = int2();

        ShaderGaussianBlur push_constant;
        push_constant.horizontal_enable = false;
        push_constant.src_texture = in_rt_0.descriptor_index;
        push_constant.src_resolution = rt_0_extent;
        //push_constant.blur_strength = m_bloom_strength;
        //push_constant.blur_scale = m_bloom_scale;

        SetPushConstantUserData(a_list, 0, sizeof(push_constant), &push_constant);

        StartRenderPass(a_list, rendering_info);
        DrawVertices(a_list, 3, 1, 0, 0);
        EndRenderPass(a_list);

        // ping pong
        to_render_target.base_array_layer = 0;
        to_shader_read.base_array_layer = 1;
        PipelineBarriers(a_list, barrier_info);
    }

    // vertical slice
    {
        FixedArray<ColorBlendState, 1> blend_state;
        blend_state[0].blend_enable = true;
        blend_state[0].color_flags = 0xF;
        blend_state[0].color_blend_op = BLEND_OP::ADD;
        blend_state[0].src_blend = BLEND_MODE::FACTOR_ONE;
        blend_state[0].dst_blend = BLEND_MODE::FACTOR_ONE;
        blend_state[0].alpha_blend_op = BLEND_OP::ADD;
        blend_state[0].src_alpha_blend = BLEND_MODE::FACTOR_SRC_ALPHA;
        blend_state[0].dst_alpha_blend = BLEND_MODE::FACTOR_DST_ALPHA;
        SetBlendMode(a_list, 0, blend_state.slice());

        RenderingAttachmentColor color_attach;
        color_attach.load_color = true;
        color_attach.store_color = true;
        color_attach.image_layout = IMAGE_LAYOUT::RT_COLOR;
        color_attach.image_view = GetImageView(out_rt.descriptor_index);
        StartRenderingInfo rendering_info;
        rendering_info.color_attachments = Slice(&color_attach, 1);
        rendering_info.depth_attachment = nullptr;
        rendering_info.layer_count = 1;
        rendering_info.view_mask = 0;
        rendering_info.render_area_extent = rt_0_extent;
        rendering_info.render_area_offset = int2{ 0, 0 };

        ShaderGaussianBlur push_constant;
        push_constant.horizontal_enable = true;
        push_constant.src_texture = in_rt_1.descriptor_index;
        push_constant.src_resolution = rt_1_extent;
        //push_constant.blur_strength = m_bloom_strength;
        //push_constant.blur_scale = m_bloom_scale;
        SetPushConstantUserData(a_list, 0, sizeof(push_constant), &push_constant);

        StartRenderPass(a_list, rendering_info);
        DrawVertices(a_list, 3, 1, 0, 0);
        EndRenderPass(a_list);
    }

    return true;
}
