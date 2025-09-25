#include "RenderStages.hpp"
#include "Renderer.hpp"
#include "MaterialSystem.hpp"

using namespace BB;

bool BB::RenderPassClearStage(RG::RenderGraph& a_graph, RG::GlobalGraphData& a_global_data, const RCommandList a_list, const MasterMaterialHandle a_material, Slice<RG::ResourceHandle> a_resource_inputs, Slice<RG::ResourceHandle> a_resource_outputs)
{
    const RG::RenderResource& skybox_texture = a_graph.GetResource(a_resource_inputs[0]);
    const RG::RenderResource& skybox_sampler = a_graph.GetResource(a_resource_inputs[1]);
    const RG::RenderResource& out_rt = a_graph.GetResource(a_resource_outputs[0]);

    a_global_data.scene_info.skybox_texture = skybox_texture.descriptor_index;
    a_global_data.scene_info.skybox_sampler = skybox_sampler.descriptor_index;

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
    return true;
}

bool BB::RenderPassShadowMapStage(RG::RenderGraph& a_graph, RG::GlobalGraphData& a_global_data, const RCommandList a_list, const MasterMaterialHandle a_material, Slice<RG::ResourceHandle>, Slice<RG::ResourceHandle> a_resource_outputs)
{
    const RG::RenderResource& out_rt = a_graph.GetResource(a_resource_outputs[0]);
    const uint2 shadow_map_extent = uint2(out_rt.image.extent.x, out_rt.image.extent.y);

    const uint32_t shadow_map_count = out_rt.image.array_layers;
    a_global_data.scene_info.shadow_map_count = shadow_map_count;
    a_global_data.scene_info.shadow_map_array_descriptor = out_rt.descriptor_index;
    a_global_data.scene_info.shadow_map_resolution = float2(static_cast<float>(shadow_map_extent.x), static_cast<float>(shadow_map_extent.y));
    if (shadow_map_count == 0)
    {
        // I don't think we need this if the graph is done
        return true;
    }

    SetPrimitiveTopology(a_list, PRIMITIVE_TOPOLOGY::TRIANGLE_LIST);
    Material::BindMaterial(a_list, a_material);

    PipelineBarrierImageInfo shadow_map_write_transition = {};
    shadow_map_write_transition.prev = IMAGE_LAYOUT::NONE;
    shadow_map_write_transition.next = IMAGE_LAYOUT::RT_DEPTH;
    shadow_map_write_transition.image = out_rt.image.image;
    shadow_map_write_transition.layer_count = shadow_map_count;
    shadow_map_write_transition.level_count = 1;
    shadow_map_write_transition.base_array_layer = out_rt.image.base_array_layer;
    shadow_map_write_transition.base_mip_level = out_rt.image.base_mip;
    shadow_map_write_transition.image_aspect = IMAGE_ASPECT::DEPTH;

    PipelineBarrierInfo write_pipeline{};
    write_pipeline.image_barriers = ConstSlice<PipelineBarrierImageInfo>(&shadow_map_write_transition, 1);
    PipelineBarriers(a_list, write_pipeline);

    RenderingAttachmentDepth depth_attach{};
    depth_attach.load_depth = false;
    depth_attach.store_depth = true;
    depth_attach.image_layout = IMAGE_LAYOUT::RT_DEPTH;
    depth_attach.image_view = GetImageView(out_rt.descriptor_index);

    StartRenderingInfo rendering_info;
    rendering_info.color_attachments = {};	// null
    rendering_info.depth_attachment = &depth_attach;
    rendering_info.layer_count = 1;
    rendering_info.view_mask = ((1u << shadow_map_count) - 1u) << out_rt.image.base_array_layer;
    rendering_info.render_area_extent = shadow_map_extent;
    rendering_info.render_area_offset = int2(0, 0);

    SetCullMode(a_list, CULL_MODE::FRONT);
    SetFrontFace(a_list, true);
    SetDepthBias(a_list, 1.25f, 0.f, 1.75f);
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

    StartRenderPass(a_list, rendering_info);
    for (uint32_t draw_index = 0; draw_index < a_graph.GetDrawList().draw_entries.size(); draw_index++)
    {
        const DrawList::DrawEntry& mesh_draw_call = a_graph.GetDrawList().draw_entries[draw_index];

        ShaderIndicesShadowMapping shader_indices;
        shader_indices.geometry_offset = static_cast<uint32_t>(mesh_draw_call.mesh.vertex_geometry_offset);
        shader_indices.transform_index = draw_index;
        SetPushConstantUserData(a_list, 0, sizeof(shader_indices), &shader_indices);
        DrawIndexed(a_list,
            mesh_draw_call.index_count,
            1,
            static_cast<uint32_t>(mesh_draw_call.mesh.index_buffer_offset / sizeof(uint32_t)) + mesh_draw_call.index_start,
            0,
            0);
    }
    EndRenderPass(a_list);

    PipelineBarrierImageInfo shadow_map_read_transition = {};
    shadow_map_read_transition.prev = IMAGE_LAYOUT::RT_DEPTH;
    shadow_map_read_transition.next = IMAGE_LAYOUT::RO_DEPTH;
    shadow_map_read_transition.image = out_rt.image.image;
    shadow_map_read_transition.layer_count = shadow_map_count;
    shadow_map_read_transition.level_count = 1;
    shadow_map_read_transition.base_array_layer = out_rt.image.base_array_layer;
    shadow_map_read_transition.base_mip_level = 0;
    shadow_map_read_transition.image_aspect = IMAGE_ASPECT::DEPTH;

    PipelineBarrierInfo pipeline_info = {};
    pipeline_info.image_barriers = ConstSlice<PipelineBarrierImageInfo>(&shadow_map_read_transition, 1);
    PipelineBarriers(a_list, pipeline_info);
    return true;
}

bool BB::RenderPassPBRStage(RG::RenderGraph& a_graph, RG::GlobalGraphData& a_global_data, const RCommandList a_list, const MasterMaterialHandle, Slice<RG::ResourceHandle> a_resource_inputs, Slice<RG::ResourceHandle> a_resource_outputs)
{
    const RG::RenderResource& matrix_buffer = a_graph.GetResource(a_resource_inputs[0]);
    const RG::RenderResource& light_buffer = a_graph.GetResource(a_resource_inputs[1]);
    const RG::RenderResource& light_view_buffer = a_graph.GetResource(a_resource_inputs[2]);

    const RG::RenderResource& out_rt = a_graph.GetResource(a_resource_outputs[0]);
    const RG::RenderResource& out_rt_bright = a_graph.GetResource(a_resource_outputs[1]);
    const RG::RenderResource& out_depth = a_graph.GetResource(a_resource_outputs[2]);
    const uint2 draw_area = uint2(out_rt.image.extent.x, out_rt.image.extent.y);
    const uint32_t light_count = static_cast<uint32_t>(light_buffer.buffer.size) / sizeof(Light);

    a_global_data.scene_info.light_count = light_count;
    a_global_data.scene_info.scene_resolution = draw_area;

    // MAYBE ALSO DO OFFSETS
    a_global_data.scene_info.matrix_offset = static_cast<uint32_t>(matrix_buffer.buffer.offset);
    a_global_data.scene_info.light_offset = static_cast<uint32_t>(light_buffer.buffer.offset);
    a_global_data.scene_info.light_view_offset = static_cast<uint32_t>(light_view_buffer.buffer.offset);

    PipelineBarrierImageInfo image_transitions[1]{};
    image_transitions[0].prev = IMAGE_LAYOUT::NONE;
    image_transitions[0].next = IMAGE_LAYOUT::RT_DEPTH;
    image_transitions[0].image = out_depth.image.image;
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
    color_attachs[0].image_view = GetImageView(out_rt.descriptor_index);

    color_attachs[1].load_color = false;
    color_attachs[1].store_color = true;
    color_attachs[1].image_layout = IMAGE_LAYOUT::RT_COLOR;
    color_attachs[1].image_view = GetImageView(out_rt_bright.descriptor_index);
    const uint32_t color_attach_count = 2;

    RenderingAttachmentDepth depth_attach{};
    depth_attach.load_depth = false;
    depth_attach.store_depth = true;
    depth_attach.image_layout = IMAGE_LAYOUT::RT_DEPTH;
    depth_attach.image_view = GetImageView(out_depth.descriptor_index);

    StartRenderingInfo rendering_info;
    rendering_info.color_attachments = color_attachs.slice(color_attach_count);
    rendering_info.depth_attachment = &depth_attach;
    rendering_info.layer_count = 1;
    rendering_info.view_mask = 0;
    rendering_info.render_area_extent = draw_area;
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

    for (uint32_t i = 0; i < a_graph.GetDrawList().draw_entries.size(); i++)
    {
        const DrawList::DrawEntry& mesh_draw_call = a_graph.GetDrawList().draw_entries[i];

        SetPrimitiveTopology(a_list, PRIMITIVE_TOPOLOGY::TRIANGLE_LIST);
        Material::BindMaterial(a_list, mesh_draw_call.master_material);

        PBRIndices shader_indices;
        shader_indices.transform_index = i;
        shader_indices.geometry_offset = static_cast<uint32_t>(mesh_draw_call.mesh.vertex_geometry_offset);
        shader_indices.shading_offset = static_cast<uint32_t>(mesh_draw_call.mesh.vertex_shading_offset);
        shader_indices.material_index = Material::GetMaterialBufferIndex(mesh_draw_call.material);
        SetPushConstantUserData(a_list, 0, sizeof(shader_indices), &shader_indices);

        DrawIndexed(a_list,
            mesh_draw_call.index_count,
            1,
            static_cast<uint32_t>(mesh_draw_call.mesh.index_buffer_offset / sizeof(uint32_t)) + mesh_draw_call.index_start,
            0,
            0);
    }

    EndRenderPass(a_list);
    return true;
}

bool BB::RenderPassLineStage(RG::RenderGraph& a_graph, RG::GlobalGraphData&, const RCommandList a_list, const MasterMaterialHandle a_material, Slice<RG::ResourceHandle> a_resource_inputs, Slice<RG::ResourceHandle> a_resource_outputs)
{
    const RG::RenderResource& line_buffer = a_graph.GetResource(a_resource_inputs[0]);

    const RG::RenderResource& out_rt = a_graph.GetResource(a_resource_outputs[0]);
    const RG::RenderResource* pdepth_buffer = nullptr;
    const uint2 draw_area = uint2(out_rt.image.extent.x, out_rt.image.extent.y);

    if (a_resource_outputs.size() == 2)
    {
        pdepth_buffer = &a_graph.GetResource(a_resource_outputs[1]);
    }

    RenderingAttachmentColor color_attach;
    color_attach.load_color = true;
    color_attach.store_color = true;
    color_attach.image_layout = IMAGE_LAYOUT::RT_COLOR;
    color_attach.image_view = GetImageView(out_rt.descriptor_index);

    RenderingAttachmentDepth* pdepth = nullptr;
    RenderingAttachmentDepth depth;
    if (pdepth_buffer)
    {
        depth = {};
        depth.load_depth = true;
        depth.store_depth = false;
        depth.image_layout = IMAGE_LAYOUT::RT_DEPTH;
        depth.image_view = GetImageView(pdepth_buffer->descriptor_index);
        pdepth = &depth;
    }

    StartRenderingInfo start_rendering_info;
    start_rendering_info.render_area_extent = draw_area;
    start_rendering_info.render_area_offset = int2{ 0, 0 };
    start_rendering_info.layer_count = 1;
    start_rendering_info.view_mask = 0;
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
    Material::BindMaterial(a_list, a_material);

    ShaderLine push_constant;
    push_constant.line_width = 1.5f;
    push_constant.vertex_start = static_cast<uint32_t>(line_buffer.buffer.offset);

    SetPushConstantUserData(a_list, 0, sizeof(push_constant), &push_constant);

    const uint32_t line_count = static_cast<uint32_t>(line_buffer.buffer.size) / sizeof(Line);

    DrawVertices(a_list, line_count * 2, 1, 0, 0);

    EndRenderPass(a_list);
    return true;
}

bool BB::RenderPassGlyphStage(RG::RenderGraph& a_graph, RG::GlobalGraphData&, const RCommandList a_list, const MasterMaterialHandle a_material, Slice<RG::ResourceHandle> a_resource_inputs, Slice<RG::ResourceHandle> a_resource_outputs)
{
    const RG::RenderResource& quad_buffer = a_graph.GetResource(a_resource_inputs[0]);
    const RG::RenderResource& out_rt = a_graph.GetResource(a_resource_outputs[0]);
    const uint2 draw_area = uint2(out_rt.image.extent.x, out_rt.image.extent.y);

    RenderingAttachmentColor color_attach;
    color_attach.load_color = true;
    color_attach.store_color = true;
    color_attach.image_layout = IMAGE_LAYOUT::RT_COLOR;
    color_attach.image_view = GetImageView(out_rt.descriptor_index);

    StartRenderingInfo start_rendering_info;
    start_rendering_info.render_area_extent = draw_area;
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
    push_constant.per_frame_buffer_start = static_cast<uint32_t>(quad_buffer.buffer.offset);

    SetPushConstantUserData(a_list, 0, sizeof(push_constant), &push_constant);

    const uint32_t quad_count = static_cast<uint32_t>(quad_buffer.buffer.size) / sizeof(Quad2D);
    DrawVertices(a_list, 6, quad_count, 1, 0);

    EndRenderPass(a_list);
    return true;
}

bool BB::RenderPassBloomStage(RG::RenderGraph& a_graph, RG::GlobalGraphData& a_global_data, const RCommandList a_list, const MasterMaterialHandle a_material, Slice<RG::ResourceHandle> a_resource_inputs, Slice<RG::ResourceHandle> a_resource_outputs)
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
        push_constant.blur_strength = a_global_data.post_fx.blur_strength;
        push_constant.blur_scale = a_global_data.post_fx.blur_scale;

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
        push_constant.blur_strength = a_global_data.post_fx.blur_strength;
        push_constant.blur_scale = a_global_data.post_fx.blur_scale;
        SetPushConstantUserData(a_list, 0, sizeof(push_constant), &push_constant);

        StartRenderPass(a_list, rendering_info);
        DrawVertices(a_list, 3, 1, 0, 0);
        EndRenderPass(a_list);
    }

    return true;
}
