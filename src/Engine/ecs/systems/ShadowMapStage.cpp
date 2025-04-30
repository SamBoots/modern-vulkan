#include "ShadowMapStage.hpp"
#include "Renderer.hpp"
#include "MaterialSystem.hpp"

using namespace BB;

constexpr uint32_t INITIAL_DEPTH_ARRAY_COUNT = 8;

void ShadowMapStage::Init(MemoryArena& a_arena, const uint32_t a_back_buffer_count)
{
    MaterialCreateInfo shadow_map_material;
    shadow_map_material.pass_type = PASS_TYPE::SCENE;
    shadow_map_material.material_type = MATERIAL_TYPE::NONE;
    MaterialShaderCreateInfo vertex_shadow_map;
    vertex_shadow_map.path = "../../resources/shaders/hlsl/ShadowMap.hlsl";
    vertex_shadow_map.entry = "VertexMain";
    vertex_shadow_map.stage = SHADER_STAGE::VERTEX;
    vertex_shadow_map.next_stages = static_cast<uint32_t>(SHADER_STAGE::NONE);
    shadow_map_material.shader_infos = Slice(&vertex_shadow_map, 1);
    MemoryArenaScope(a_arena)
    {
        m_shadowmap_material = Material::CreateMasterMaterial(a_arena, shadow_map_material, "shadow map material");
    }

    m_per_frame.Init(a_arena, a_back_buffer_count);
    m_per_frame.resize(a_back_buffer_count);
    for (uint32_t i = 0; i < m_per_frame.size(); i++)
    {
        PerFrame& pfd = m_per_frame[i];
        pfd.render_pass_views.Init(a_arena, INITIAL_DEPTH_ARRAY_COUNT);
        pfd.render_pass_views.resize(INITIAL_DEPTH_ARRAY_COUNT);
        {
            ImageCreateInfo shadow_map_img;
            shadow_map_img.name = "shadow map array";
            shadow_map_img.width = DEPTH_IMAGE_SIZE_W_H;
            shadow_map_img.height = DEPTH_IMAGE_SIZE_W_H;
            shadow_map_img.depth = 1;
            shadow_map_img.array_layers = static_cast<uint16_t>(pfd.render_pass_views.size());
            shadow_map_img.mip_levels = 1;
            shadow_map_img.use_optimal_tiling = true;
            shadow_map_img.type = IMAGE_TYPE::TYPE_2D;
            shadow_map_img.format = IMAGE_FORMAT::D16_UNORM;
            shadow_map_img.usage = IMAGE_USAGE::SHADOW_MAP;
            shadow_map_img.is_cube_map = false;
            pfd.image = CreateImage(shadow_map_img);
        }

        {
            ImageViewCreateInfo shadow_map_img_view;
            shadow_map_img_view.name = "shadow map array view";
            shadow_map_img_view.image = pfd.image;
            shadow_map_img_view.base_array_layer = 0;
            shadow_map_img_view.array_layers = static_cast<uint16_t>(pfd.render_pass_views.size());
            shadow_map_img_view.mip_levels = 1;
            shadow_map_img_view.base_mip_level = 0;
            shadow_map_img_view.format = IMAGE_FORMAT::D16_UNORM;
            shadow_map_img_view.type = IMAGE_VIEW_TYPE::TYPE_2D_ARRAY;
            shadow_map_img_view.aspects = IMAGE_ASPECT::DEPTH;
            pfd.descriptor_index = CreateImageView(shadow_map_img_view);
        }

        {
            ImageViewCreateInfo render_pass_shadow_view{};
            render_pass_shadow_view.name = "shadow map renderpass view";
            render_pass_shadow_view.image = pfd.image;
            render_pass_shadow_view.array_layers = 1;
            render_pass_shadow_view.mip_levels = 1;
            render_pass_shadow_view.base_mip_level = 0;
            render_pass_shadow_view.format = IMAGE_FORMAT::D16_UNORM;
            render_pass_shadow_view.type = IMAGE_VIEW_TYPE::TYPE_2D;
            render_pass_shadow_view.aspects = IMAGE_ASPECT::DEPTH;
            for (uint32_t shadow_index = 0; shadow_index < pfd.render_pass_views.size(); shadow_index++)
            {
                render_pass_shadow_view.base_array_layer = static_cast<uint16_t>(shadow_index);
                pfd.render_pass_views[shadow_index] = CreateImageViewShaderInaccessible(render_pass_shadow_view);
            }
        }
    }
}

void ShadowMapStage::ExecutePass(const RCommandList a_list, const uint32_t a_frame_index, const uint2 a_shadow_map_resolution, const DrawList& a_draw_list, const ConstSlice<LightComponent> a_lights)
{
    PerFrame& pfd = m_per_frame[a_frame_index];

    const uint32_t shadow_map_count = static_cast<uint32_t>(a_lights.size());
    if (shadow_map_count == 0)
    {
        return;
    }
    BB_ASSERT(shadow_map_count <= pfd.render_pass_views.size(), "too many lights! Make a dynamic shadow mapping array");

    const RPipelineLayout pipe_layout = BindShaders(a_list, Material::GetMaterialShaders(m_shadowmap_material));

    PipelineBarrierImageInfo shadow_map_write_transition = {};
    shadow_map_write_transition.prev = IMAGE_LAYOUT::NONE;
    shadow_map_write_transition.next = IMAGE_LAYOUT::RT_DEPTH;
    shadow_map_write_transition.image = pfd.image;
    shadow_map_write_transition.layer_count = shadow_map_count;
    shadow_map_write_transition.level_count = 1;
    shadow_map_write_transition.base_array_layer = 0;
    shadow_map_write_transition.base_mip_level = 0;
    shadow_map_write_transition.image_aspect = IMAGE_ASPECT::DEPTH;

    PipelineBarrierInfo write_pipeline{};
    write_pipeline.image_barriers = ConstSlice<PipelineBarrierImageInfo>(&shadow_map_write_transition, 1);
    PipelineBarriers(a_list, write_pipeline);

    RenderingAttachmentDepth depth_attach{};
    depth_attach.load_depth = false;
    depth_attach.store_depth = true;
    depth_attach.image_layout = IMAGE_LAYOUT::RT_DEPTH;

    StartRenderingInfo rendering_info;
    rendering_info.color_attachments = {};	// null
    rendering_info.depth_attachment = &depth_attach;
    rendering_info.render_area_extent = a_shadow_map_resolution;
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

    for (uint32_t shadow_map_index = 0; shadow_map_index < shadow_map_count; shadow_map_index++)
    {
        depth_attach.image_view = pfd.render_pass_views[shadow_map_index];

        StartRenderPass(a_list, rendering_info);
        for (uint32_t draw_index = 0; draw_index < a_draw_list.draw_entries.size(); draw_index++)
        {
            const DrawList::DrawEntry& mesh_draw_call = a_draw_list.draw_entries[draw_index];

            ShaderIndicesShadowMapping shader_indices;
            shader_indices.position_offset = static_cast<uint32_t>(mesh_draw_call.mesh.vertex_position_offset);
            shader_indices.transform_index = draw_index;
            shader_indices.light_projection_view_index = shadow_map_index;
            SetPushConstants(a_list, pipe_layout, 0, sizeof(shader_indices), &shader_indices);
            DrawIndexed(a_list,
                mesh_draw_call.index_count,
                1,
                static_cast<uint32_t>(mesh_draw_call.mesh.index_buffer_offset / sizeof(uint32_t)) + mesh_draw_call.index_start,
                0,
                0);
        }
        EndRenderPass(a_list);
    }

    PipelineBarrierImageInfo shadow_map_read_transition = {};
    shadow_map_read_transition.prev = IMAGE_LAYOUT::RT_DEPTH;
    shadow_map_read_transition.next = IMAGE_LAYOUT::RO_DEPTH;
    shadow_map_read_transition.image = pfd.image;
    shadow_map_read_transition.layer_count = shadow_map_count;
    shadow_map_read_transition.level_count = 1;
    shadow_map_read_transition.base_array_layer = 0;
    shadow_map_read_transition.base_mip_level = 0;
    shadow_map_read_transition.image_aspect = IMAGE_ASPECT::DEPTH;

    PipelineBarrierInfo pipeline_info = {};
    pipeline_info.image_barriers = ConstSlice<PipelineBarrierImageInfo>(&shadow_map_read_transition, 1);
    PipelineBarriers(a_list, pipeline_info);
}

void ShadowMapStage::UpdateConstantBuffer(const uint32_t a_frame_index, Scene3DInfo& a_scene_3d_info) const
{
    const PerFrame& pfd = m_per_frame[a_frame_index];
    a_scene_3d_info.shadow_map_count = pfd.render_pass_views.size();
    a_scene_3d_info.shadow_map_array_descriptor = pfd.descriptor_index;
}
