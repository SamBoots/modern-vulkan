#include "BloomStage.hpp"
#include "MaterialSystem.hpp"
#include "Renderer.hpp"

using namespace BB;

void BloomStage::Init(MemoryArena& a_arena)
{
    MaterialCreateInfo gaussian_material;
    gaussian_material.pass_type = PASS_TYPE::SCENE;
    gaussian_material.material_type = MATERIAL_TYPE::NONE;
    FixedArray<MaterialShaderCreateInfo, 2> gaussian_shaders;
    gaussian_shaders[0].path = "hlsl/GaussianBlur.hlsl";
    gaussian_shaders[0].entry = "VertexMain";
    gaussian_shaders[0].stage = SHADER_STAGE::VERTEX;
    gaussian_shaders[0].next_stages = static_cast<uint32_t>(SHADER_STAGE::FRAGMENT_PIXEL);
    gaussian_shaders[1].path = "hlsl/GaussianBlur.hlsl";
    gaussian_shaders[1].entry = "FragmentMain";
    gaussian_shaders[1].stage = SHADER_STAGE::FRAGMENT_PIXEL;
    gaussian_shaders[1].next_stages = static_cast<uint32_t>(SHADER_STAGE::NONE);
    gaussian_material.shader_infos = Slice(gaussian_shaders.slice());

    MemoryArenaScope(a_arena)
    {
        m_gaussian_material = Material::CreateMasterMaterial(a_arena, gaussian_material, "shadow map material");
    }

    m_bloom_scale = 1.0f;
    m_bloom_strength = 1.5f;
}

void BloomStage::ExecutePass(const RCommandList a_list, const uint2 a_resolution, const RImage a_render_target_image, const RDescriptorIndex a_render_target_0, const RDescriptorIndex a_render_target_1, const uint2 a_draw_area, const RImageView a_render_target)
{
    SetPrimitiveTopology(a_list, PRIMITIVE_TOPOLOGY::TRIANGLE_LIST);
    Material::BindMaterial(a_list, m_gaussian_material);

    FixedArray<PipelineBarrierImageInfo, 2> transitions{};
    PipelineBarrierImageInfo& to_shader_read = transitions[0];
    to_shader_read.prev = IMAGE_LAYOUT::RT_COLOR;
    to_shader_read.next = IMAGE_LAYOUT::RO_FRAGMENT;
    to_shader_read.image = a_render_target_image;
    to_shader_read.layer_count = 1;
    to_shader_read.level_count = 1;
    to_shader_read.base_array_layer = 0;
    to_shader_read.base_mip_level = 0;
    to_shader_read.image_aspect = IMAGE_ASPECT::COLOR;

    PipelineBarrierImageInfo& to_render_target = transitions[1];
    to_render_target.prev = IMAGE_LAYOUT::RO_FRAGMENT;
    to_render_target.next = IMAGE_LAYOUT::RT_COLOR;
    to_render_target.image = a_render_target_image;
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
        color_attach.image_view = GetImageView(a_render_target_1);
        StartRenderingInfo rendering_info;
        rendering_info.color_attachments = Slice(&color_attach, 1);
        rendering_info.depth_attachment = nullptr;
        rendering_info.render_area_extent = a_resolution;
        rendering_info.render_area_offset = int2();

        ShaderGaussianBlur push_constant;
        push_constant.horizontal_enable = false;
        push_constant.src_texture = a_render_target_0;
        push_constant.src_resolution = a_resolution;
        push_constant.blur_strength = m_bloom_strength;
        push_constant.blur_scale = m_bloom_scale;

        SetPushConstantUserData(a_list, sizeof(push_constant), &push_constant);

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
        color_attach.image_view = a_render_target;
        StartRenderingInfo rendering_info;
        rendering_info.color_attachments = Slice(&color_attach, 1);
        rendering_info.depth_attachment = nullptr;
        rendering_info.render_area_extent = a_draw_area;
        rendering_info.render_area_offset = int2{ 0, 0 };

        ShaderGaussianBlur push_constant;
        push_constant.horizontal_enable = true;
        push_constant.src_texture = a_render_target_1;
        push_constant.src_resolution = a_resolution;
        push_constant.blur_strength = m_bloom_strength;
        push_constant.blur_scale = m_bloom_scale;
        SetPushConstantUserData(a_list, sizeof(push_constant), &push_constant);

        StartRenderPass(a_list, rendering_info);
        DrawVertices(a_list, 3, 1, 0, 0);
        EndRenderPass(a_list);
    }
}
