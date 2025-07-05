#include "ClearStage.hpp"
#include "Renderer.hpp"
#include "MaterialSystem.hpp"
#include "AssetLoader.hpp"

using namespace BB;

void ClearStage::Init(MemoryArena& a_arena)
{
    ImageCreateInfo skybox_image_info;
    skybox_image_info.name = "skybox image";
    skybox_image_info.width = 2048;
    skybox_image_info.height = 2048;
    skybox_image_info.depth = 1;
    skybox_image_info.array_layers = 6;
    skybox_image_info.mip_levels = 1;
    skybox_image_info.use_optimal_tiling = true;
    skybox_image_info.type = IMAGE_TYPE::TYPE_2D;
    skybox_image_info.format = IMAGE_FORMAT::RGBA8_SRGB;
    skybox_image_info.usage = IMAGE_USAGE::TEXTURE;
    skybox_image_info.is_cube_map = true;
    m_skybox = CreateImage(skybox_image_info);

    ImageViewCreateInfo skybox_image_view_info;
    skybox_image_view_info.name = "skybox image view";
    skybox_image_view_info.image = m_skybox;
    skybox_image_view_info.base_array_layer = 0;
    skybox_image_view_info.mip_levels = 1;
    skybox_image_view_info.array_layers = 6;
    skybox_image_view_info.base_mip_level = 0;
    skybox_image_view_info.format = IMAGE_FORMAT::RGBA8_SRGB;
    skybox_image_view_info.type = IMAGE_VIEW_TYPE::CUBE;
    skybox_image_view_info.aspects = IMAGE_ASPECT::COLOR;
    m_skybox_descriptor_index = CreateImageView(skybox_image_view_info);

    MemoryArenaScope(a_arena)
    {
        constexpr StringView SKYBOX_NAMES[6]
        {
            StringView("skybox/0.jpg"),
            StringView("skybox/1.jpg"),
            StringView("skybox/2.jpg"),
            StringView("skybox/3.jpg"),
            StringView("skybox/4.jpg"),
            StringView("skybox/5.jpg")
        };

        FixedArray<Asset::AsyncAsset, 6> skybox_textures;

        MaterialCreateInfo skybox_material;
        skybox_material.pass_type = PASS_TYPE::SCENE;
        skybox_material.material_type = MATERIAL_TYPE::NONE;
        FixedArray<MaterialShaderCreateInfo, 2> skybox_shaders;
        skybox_shaders[0].path = "hlsl/skybox.hlsl";
        skybox_shaders[0].entry = "VertexMain";
        skybox_shaders[0].stage = SHADER_STAGE::VERTEX;
        skybox_shaders[0].next_stages = static_cast<uint32_t>(SHADER_STAGE::FRAGMENT_PIXEL);
        skybox_shaders[1].path = "hlsl/skybox.hlsl";
        skybox_shaders[1].entry = "FragmentMain";
        skybox_shaders[1].stage = SHADER_STAGE::FRAGMENT_PIXEL;
        skybox_shaders[1].next_stages = static_cast<uint32_t>(SHADER_STAGE::NONE);
        skybox_material.shader_infos = Slice(skybox_shaders.slice());

        const Image& skybox_image = Asset::LoadImageArrayDisk(a_arena, "default skybox", ConstSlice<StringView>(SKYBOX_NAMES, _countof(SKYBOX_NAMES)), IMAGE_FORMAT::RGBA8_SRGB, true);
        m_skybox = skybox_image.gpu_image;
        m_skybox_descriptor_index = skybox_image.descriptor_index;
        m_skybox_material = Material::CreateMasterMaterial(a_arena, skybox_material, "skybox material");
    }
}

void ClearStage::ExecutePass(const RCommandList a_list, const uint2 a_draw_area_size, const RImageView a_render_target)
{
    SetPrimitiveTopology(a_list, PRIMITIVE_TOPOLOGY::TRIANGLE_LIST);
    Material::BindMaterial(a_list, m_skybox_material);

    RenderingAttachmentColor color_attach;
    color_attach.load_color = false;
    color_attach.store_color = true;
    color_attach.image_layout = IMAGE_LAYOUT::RT_COLOR;
    color_attach.image_view = a_render_target;

    StartRenderingInfo start_rendering_info;
    start_rendering_info.render_area_extent = a_draw_area_size;
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

    SetFrontFace(a_list, false);
    SetCullMode(a_list, CULL_MODE::NONE);

    DrawCubemap(a_list, 1, 0);

    EndRenderPass(a_list);
}

void ClearStage::UpdateConstantBuffer(Scene3DInfo& a_scene_3d_info) const
{
    a_scene_3d_info.skybox_texture = m_skybox_descriptor_index;
}
