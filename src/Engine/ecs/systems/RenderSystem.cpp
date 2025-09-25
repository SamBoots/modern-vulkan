#include "RenderSystem.hpp"
#include "MaterialSystem.hpp"
#include "Math/Math.inl"
#include "Renderer.hpp"

#include "AssetLoader.hpp"

#include "Engine.hpp"

#include "RenderStages.hpp"

using namespace BB;

constexpr float BLOOM_IMAGE_DOWNSCALE_FACTOR = 1.f;
constexpr uint32_t DEPTH_IMAGE_SIZE_W_H = 1024;

void RenderSystem::Init(MemoryArena& a_arena, const uint32_t a_back_buffer_count, const uint2 a_render_target_size)
{
    m_frame_count = a_back_buffer_count;
    m_final_image_extent = a_render_target_size;
    m_final_image_format = IMAGE_FORMAT::RGBA8_SRGB;

	m_options.skip_skybox = false;
	m_options.skip_shadow_mapping = false;
	m_options.skip_object_rendering = false;
	m_options.skip_bloom = false;

    // IF USING RAYTRACING
    if (false)
    {
        GPUBufferCreateInfo accel_create_info;
        accel_create_info.name = "acceleration structure buffer";
        accel_create_info.size = mbSize * 256;
        accel_create_info.type = BUFFER_TYPE::RT_ACCELERATION;
        accel_create_info.host_writable = false;
        m_raytrace_data.acceleration_structure_buffer.Init(accel_create_info);

        GPUBufferCreateInfo top_level_instance_buffer;
        top_level_instance_buffer.name = "acceleration structure host visible build buffer";
        top_level_instance_buffer.size = mbSize * 64;
        top_level_instance_buffer.type = BUFFER_TYPE::RT_BUILD_ACCELERATION;
        top_level_instance_buffer.host_writable = true;

		m_raytrace_data.top_level.build_info.build_buffer.Init(top_level_instance_buffer);
		m_raytrace_data.top_level.build_info.build_mapped = MapGPUBuffer(m_raytrace_data.top_level.build_info.build_buffer.GetBuffer());
		m_raytrace_data.top_level.build_info.build_address = GetGPUBufferAddress(m_raytrace_data.top_level.build_info.build_buffer.GetBuffer());
		m_raytrace_data.top_level.must_update = false;
		m_raytrace_data.top_level.must_rebuild = false;
    }

    PathString font_string = GetRootPath();
    font_string.AddPathNoSlash("resources/fonts/ProggyVector.ttf");
    PathString write_image = GetRootPath();
    write_image.AddPathNoSlash("resources/fonts/ProggyVector.png");

    m_font_atlas = CreateFontAtlas(a_arena, font_string, 32, 0);

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

    MaterialCreateInfo shadow_map_material;
    shadow_map_material.pass_type = PASS_TYPE::SCENE;
    shadow_map_material.material_type = MATERIAL_TYPE::NONE;
    MaterialShaderCreateInfo vertex_shadow_map;
    vertex_shadow_map.path = "hlsl/ShadowMap.hlsl";
    vertex_shadow_map.entry = "VertexMain";
    vertex_shadow_map.stage = SHADER_STAGE::VERTEX;
    vertex_shadow_map.next_stages = static_cast<uint32_t>(SHADER_STAGE::NONE);
    shadow_map_material.shader_infos = Slice(&vertex_shadow_map, 1);

    MaterialCreateInfo glyph_material;
    glyph_material.pass_type = PASS_TYPE::SCENE;
    glyph_material.material_type = MATERIAL_TYPE::MATERIAL_2D;
    glyph_material.cpu_writeable = false;
    glyph_material.user_data_size = 0;
    FixedArray<MaterialShaderCreateInfo, 2> glyph_shaders;
    glyph_shaders[0].stage = SHADER_STAGE::VERTEX;
    glyph_shaders[0].next_stages = static_cast<uint32_t>(SHADER_STAGE::FRAGMENT_PIXEL);
    glyph_shaders[0].entry = "VertexMain";
    glyph_shaders[0].path = "HLSL/glyph.hlsl";
    glyph_shaders[1].stage = SHADER_STAGE::FRAGMENT_PIXEL;
    glyph_shaders[1].next_stages = static_cast<uint32_t>(SHADER_STAGE::NONE);
    glyph_shaders[1].entry = "FragmentMain";
    glyph_shaders[1].path = "HLSL/glyph.hlsl";
    glyph_material.shader_infos = glyph_shaders.slice();

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
        m_skybox_material = Material::CreateMasterMaterial(a_arena, skybox_material, "clear material");
        m_shadowmap_material = Material::CreateMasterMaterial(a_arena, shadow_map_material, "shadowmap material");
        m_glyph_material = Material::CreateMasterMaterial(a_arena, glyph_material, "glyph material");
        m_line_material = Material::CreateMasterMaterial(a_arena, line_material, "line material");
        m_gaussian_material = Material::CreateMasterMaterial(a_arena, gaussian_material, "shadow map material");

        constexpr StringView SKYBOX_NAMES[6]
        {
            StringView("skybox/0.jpg"),
            StringView("skybox/1.jpg"),
            StringView("skybox/2.jpg"),
            StringView("skybox/3.jpg"),
            StringView("skybox/4.jpg"),
            StringView("skybox/5.jpg")
        };

        const Image& skybox_image = Asset::LoadImageArrayDisk(a_arena, "default skybox", ConstSlice<StringView>(SKYBOX_NAMES, _countof(SKYBOX_NAMES)), IMAGE_FORMAT::RGBA8_SRGB, true);
        m_skybox = skybox_image.gpu_image;
        m_skybox_descriptor_index = skybox_image.descriptor_index;
    }

    m_skybox_sampler_index = AllocateSamplerDescriptor();
    SamplerCreateInfo sampler_info;
    sampler_info.name = "skybox sampler";
    sampler_info.mode_u = SAMPLER_ADDRESS_MODE::CLAMP;
    sampler_info.mode_v = SAMPLER_ADDRESS_MODE::CLAMP;
    sampler_info.mode_w = SAMPLER_ADDRESS_MODE::CLAMP;
    sampler_info.filter = SAMPLER_FILTER::LINEAR;
    sampler_info.max_anistoropy = 16.f;
    sampler_info.min_lod = 0.0f;
    sampler_info.max_lod = 0.0f;
    sampler_info.border_color = SAMPLER_BORDER_COLOR::COLOR_FLOAT_TRANSPARENT_BLACK;
    m_skybox_sampler = CreateSampler(sampler_info);
    DescriptorWriteSampler(m_skybox_descriptor_index, m_skybox_sampler);

    m_graph_system.Init(a_arena, a_back_buffer_count, 10, 100);
    m_graph_system.GetGlobalData().scene_info.ambient_light = float4(0.03f, 0.03f, 0.03f, 1.f);
    m_graph_system.GetGlobalData().scene_info.exposure = 1.0;
    m_graph_system.GetGlobalData().scene_info.shadow_map_resolution = float2(DEPTH_IMAGE_SIZE_W_H, DEPTH_IMAGE_SIZE_W_H);

}

void RenderSystem::StartFrame(MemoryArena& a_per_frame_arena, const uint32_t a_max_ui_elements)
{
    //PerFrame& pfd = m_per_frame[m_current_frame];
    //pfd.per_frame_buffer.Clear();
    m_ui_stage.BeginDraw(a_per_frame_arena, a_max_ui_elements);
}

RenderSystemFrame RenderSystem::EndFrame(const RCommandList a_list, const IMAGE_LAYOUT a_current_layout)
{
    const RG::RenderResource& final_image = m_cur_graph->GetResource(m_final_image);

	PipelineBarrierImageInfo render_target_transition;
	render_target_transition.prev = a_current_layout;
	render_target_transition.next = IMAGE_LAYOUT::RO_FRAGMENT;
	render_target_transition.image = final_image.image.image;
	render_target_transition.layer_count = 1;
	render_target_transition.level_count = 1;
	render_target_transition.base_array_layer = 0;
	render_target_transition.base_mip_level = 0;
	render_target_transition.image_aspect = IMAGE_ASPECT::COLOR;

	PipelineBarrierInfo pipeline_info{};
	pipeline_info.image_barriers = ConstSlice<PipelineBarrierImageInfo>(&render_target_transition, 1);
	PipelineBarriers(a_list, pipeline_info);

    m_graph_system.EndGraph(*m_cur_graph);

	RenderSystemFrame frame;
	frame.render_target = final_image.descriptor_index;
	m_current_frame = (m_current_frame + 1) % m_frame_count;

	frame.fence = m_graph_system.GetFence();
	frame.fence_value = m_graph_system.IncrementNextFenceValue();
	return frame;
}

void RenderSystem::UpdateRenderSystem(MemoryArena& a_per_frame_arena, const RCommandList a_list, const uint2 a_draw_area, const WorldMatrixComponentPool& a_world_matrices, const RenderComponentPool& a_render_pool, const RaytraceComponentPool& a_raytrace_pool, const ConstSlice<LightComponent> a_lights)
{
    const ConstSlice<ECSEntity> render_entities = a_render_pool.GetEntityComponents();
    const size_t render_component_count = a_render_pool.GetEntityComponents().size();

    m_graph_system.StartGraph(a_per_frame_arena, m_current_frame, m_cur_graph, static_cast<uint32_t>(render_component_count));
    
    BindGraphicsBindlessSet(a_list);
    BindIndexBuffer(a_list, 0);
    SetPushConstantsSceneUniformIndex(a_list, m_cur_graph->GetPerFrameBufferDescriptorIndex());
    
    for (size_t i = 0; i < render_component_count; i++)
    {
        RenderComponent& comp = a_render_pool.GetComponent(render_entities[i]);
        const float4x4& transform = a_world_matrices.GetComponent(render_entities[i]);

        if (comp.material_dirty)
        {
            const GPUBufferView upload_view = m_graph_system.AllocateAndUploadGPUMemory(sizeof(comp.material_data), &comp.material_data);
            Material::WriteMaterial(comp.material, a_list, upload_view.buffer, upload_view.offset);
            comp.material_dirty = false;
        }

        DrawList::DrawEntry entry;
        entry.mesh = comp.mesh;
        entry.master_material = comp.master_material;
        entry.material = comp.material;
        entry.index_start = comp.index_start;
        entry.index_count = comp.index_count;

        ShaderTransform shader_transform;
        shader_transform.transform = transform;
        shader_transform.inverse = Float4x4Inverse(transform);

        m_cur_graph->AddDrawEntry(entry, shader_transform);
    }

    const uint3 render_target_size = uint3(m_final_image_extent.x, m_final_image_extent.y, 1);
    m_final_image = m_cur_graph->AddImage("final image",
        render_target_size, 
        1, 
        1, 
        IMAGE_USAGE::RENDER_TARGET, 
        m_final_image_format);

    const RG::ResourceHandle skybox_texture = m_cur_graph->AddTexture("skybox",
        m_skybox,
        m_skybox_descriptor_index,
        render_target_size,
        6,
        1,
        IMAGE_FORMAT::RGB8_SRGB,
        true);

    const RG::ResourceHandle skybox_sampler = m_cur_graph->AddSampler("skybox sampler", m_skybox_sampler_index);

    RG::RenderPass& clear_pass = m_cur_graph->AddRenderPass(a_per_frame_arena, RenderPassClearStage, 2, 1, m_skybox_material);
    clear_pass.AddInputResource(skybox_texture); // texture
    clear_pass.AddInputResource(skybox_sampler); // sampler
    clear_pass.AddOutputResource(m_final_image);

    //if (a_lights.size())
    //{
    //    const RG::ResourceHandle shadowmaps = m_cur_graph->AddImage("shadow maps",
    //        uint3(DEPTH_IMAGE_SIZE_W_H, DEPTH_IMAGE_SIZE_W_H, 1), 
    //        static_cast<uint16_t>(a_lights.size()), 
    //        1,
    //        IMAGE_USAGE::SHADOW_MAP,
    //        IMAGE_FORMAT::D16_UNORM);
    //    RG::RenderPass& shadowmap_pass = m_cur_graph->AddRenderPass(a_per_frame_arena, RenderPassShadowMapStage, 0, 1, m_shadowmap_material);
    //    shadowmap_pass.AddOutputResource(shadowmaps);
    //}

    //const DrawList& draw_list = m_cur_graph->GetDrawList();
    //const RG::ResourceHandle matrix_buffer = m_cur_graph->AddBuffer("matrix buffer", draw_list.transforms.size() * sizeof(ShaderTransform), draw_list.transforms.data());

    //RG::ResourceHandle light_buffer;
    //RG::ResourceHandle light_view_buffer;

    //if (a_lights.size())
    //{
    //    // temp make light and light view seperate
    //    StaticArray<Light> lights{};
    //    StaticArray<float4x4> light_view{};
    //    lights.Init(a_per_frame_arena, static_cast<uint32_t>(a_lights.size()));
    //    light_view.Init(a_per_frame_arena, static_cast<uint32_t>(a_lights.size()));
    //    for (size_t i = 0; i < a_lights.size(); i++)
    //    {
    //        lights.push_back(a_lights[i].light);
    //        light_view.push_back(a_lights[i].projection_view);
    //    }
    //    light_buffer = m_cur_graph->AddBuffer("light buffer", lights.size() * sizeof(Light), lights.data());
    //    light_view_buffer = m_cur_graph->AddBuffer("light view buffer", light_view.size() * sizeof(float4x4), light_view.data());
    //}
    //else
    //{
    //    light_buffer = m_cur_graph->AddBuffer("light buffer", 0);
    //    light_view_buffer = m_cur_graph->AddBuffer("light view buffer", 0);
    //}
   

    //const RG::ResourceHandle bright_image = m_cur_graph->AddImage("bright image",
    //    render_target_size, 1, 1, IMAGE_USAGE::RENDER_TARGET, m_final_image_format);
    //const RG::ResourceHandle depth_buffer = m_cur_graph->AddImage("depth buffer",
    //    render_target_size, 1, 1, IMAGE_USAGE::DEPTH, IMAGE_FORMAT::D24_UNORM_S8_UINT);

    //RG::RenderPass& pbr_pass = m_cur_graph->AddRenderPass(a_per_frame_arena, RenderPassPBRStage, 3, 3, MasterMaterialHandle());
    //pbr_pass.AddInputResource(matrix_buffer);
    //pbr_pass.AddInputResource(light_buffer);
    //pbr_pass.AddInputResource(light_view_buffer);
    //pbr_pass.AddOutputResource(m_final_image);
    //pbr_pass.AddOutputResource(bright_image);
    //pbr_pass.AddOutputResource(depth_buffer);
    //
    ////RG::RenderPass& debug_pass = pgraph->AddRenderPass(a_per_frame_arena, RenderPassLineStage, 1, 2, m_line_material);
    ////debug_pass.AddInputResource(lines);
    ////debug_pass.AddOutputResource(final_image);
    ////debug_pass.AddOutputResource(depth_buffer);

    //ConstSlice quads = m_ui_stage.GetQuads();
    //if (quads.size())
    //{
    //    const RG::ResourceHandle quad_buffer = m_cur_graph->AddBuffer("quads", quads.sizeInBytes(), quads.data());

    //    RG::RenderPass& glyph_pass = m_cur_graph->AddRenderPass(a_per_frame_arena, RenderPassGlyphStage, 1, 1, m_glyph_material);
    //    glyph_pass.AddInputResource(quad_buffer);
    //    glyph_pass.AddOutputResource(m_final_image);
    //}


    //const RG::ResourceHandle ping = m_cur_graph->AddImage("ping image",
    //    render_target_size,
    //    1,
    //    1,
    //    IMAGE_USAGE::RENDER_TARGET,
    //    m_final_image_format);
    //const RG::ResourceHandle pong = m_cur_graph->AddImage("pong image",
    //    render_target_size,
    //    1,
    //    1,
    //    IMAGE_USAGE::RENDER_TARGET,
    //    m_final_image_format);
    //RG::RenderPass& bloom_pass = m_cur_graph->AddRenderPass(a_per_frame_arena, RenderPassBloomStage, 2, 1, m_gaussian_material);
    //bloom_pass.AddInputResource(ping);
    //bloom_pass.AddInputResource(pong);
    //bloom_pass.AddOutputResource(m_final_image);

    m_graph_system.CompileGraph(a_per_frame_arena, *m_cur_graph);
    m_graph_system.ExecuteGraph(a_per_frame_arena, a_list, *m_cur_graph);
}

void RenderSystem::Resize(const uint2 a_new_extent, const bool a_force)
{
	if (m_final_image_extent == a_new_extent && !a_force)
		return;

	// wait until the rendering is all done
    m_graph_system.WaitFence();
	GPUWaitIdle();

    m_final_image_extent = a_new_extent;
}

void RenderSystem::ResizeNewFormat(const uint2 a_render_target_size, const IMAGE_FORMAT a_render_target_format)
{
	Resize(a_render_target_size, true);
    m_final_image_format = a_render_target_format;
}

void RenderSystem::Screenshot(const PathString& a_path) const
{
	// wait until the rendering is all done
    m_graph_system.WaitFence();
    // todo, remember the previous frame's image, should be easy.
    // 
	//const uint32_t prev_frame = (m_current_frame + 1) % m_per_frame.size();

	//ImageInfo image_info;
	//image_info.image = m_render_target.image;
	//image_info.extent = m_render_target.extent;
	//image_info.offset = int2(0);
	//image_info.array_layers = 1;
	//image_info.base_array_layer = static_cast<uint16_t>(prev_frame);
	//image_info.mip_level = 0;

	//const bool success = Asset::ReadWriteTextureDeferred(a_path.GetView(), image_info);
	//BB_ASSERT(success, "failed to write screenshot image to disk");
}

void RenderSystem::SetView(const float4x4& a_view, const float3& a_view_position)
{
	m_graph_system.GetGlobalData().scene_info.view = a_view;
    m_graph_system.GetGlobalData().scene_info.view_pos = float3(a_view_position.x, a_view_position.y, a_view_position.z);
}

void RenderSystem::SetProjection(const float4x4& a_projection, const float a_near_plane)
{
    m_graph_system.GetGlobalData().scene_info.proj = a_projection;
    m_graph_system.GetGlobalData().scene_info.near_plane = a_near_plane;
}

void RenderSystem::BuildTopLevelAccelerationStructure(MemoryArena& a_per_frame_arena, const RCommandList a_list, const ConstSlice<AccelerationStructureInstanceInfo> a_instances)
{
	RaytraceData::TopLevel& tpl = m_raytrace_data.top_level;
	// TEMP STUFF 
	GPUBufferView view;
	bool success = tpl.build_info.build_buffer.Allocate(AccelerationStructureInstanceUploadSize() * a_instances.size(), view);
	BB_ASSERT(success, "failed to allocate memory for a top level acceleration structure build info");
	success = UploadAccelerationStructureInstances(Pointer::Add(tpl.build_info.build_mapped, view.offset), view.size, a_instances);

	const AccelerationStructSizeInfo size_info = GetTopLevelAccelerationStructSizeInfo(a_per_frame_arena, ConstSlice<GPUAddress>(&tpl.build_info.build_address, 1));
	if (tpl.accel_buffer_view.size < size_info.acceleration_structure_size)
	{
		success = m_raytrace_data.acceleration_structure_buffer.Allocate(size_info.acceleration_structure_size * 2, tpl.accel_buffer_view);
		BB_ASSERT(success, "failed to allocate memory for a top level acceleration structure");
	}

	tpl.build_size = size_info.acceleration_structure_size;
	tpl.scratch_size = size_info.scratch_build_size;
	tpl.scratch_update = size_info.scratch_update_size;
	// not yet, this is wrong
	tpl.accel_struct = CreateTopLevelAccelerationStruct(size_info.acceleration_structure_size, tpl.accel_buffer_view.buffer, tpl.accel_buffer_view.offset);
}
