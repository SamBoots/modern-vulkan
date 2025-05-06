#include "RenderSystem.hpp"
#include "MaterialSystem.hpp"
#include "Math/Math.inl"
#include "Renderer.hpp"

#include "AssetLoader.hpp"

using namespace BB;

constexpr float BLOOM_IMAGE_DOWNSCALE_FACTOR = 1.f;

void RenderSystem::Init(MemoryArena& a_arena, const uint32_t a_back_buffer_count, const uint32_t a_max_lights, const uint2 a_render_target_size)
{
	m_global_buffer.light_max = a_max_lights;
	GPUBufferCreateInfo buff_create;
	buff_create.name = "light buffer";
	buff_create.size = a_max_lights * (sizeof(Light) + sizeof(float4x4));
	buff_create.type = BUFFER_TYPE::UNIFORM;
	buff_create.host_writable = false;
	m_global_buffer.buffer.Init(buff_create);

	m_global_buffer.buffer.Allocate(a_max_lights * sizeof(Light), m_global_buffer.light_view);
	m_global_buffer.buffer.Allocate(a_max_lights * sizeof(float4x4), m_global_buffer.light_viewproj_view);

	m_fence = CreateFence(0, "scene fence");
	m_last_completed_fence_value = 0;
	m_next_fence_value = 1;

	m_upload_allocator.Init(a_arena, mbSize * 4, m_fence, "scene upload buffer");

	m_scene_info.ambient_light = float4(0.03f, 0.03f, 0.03f, 1.f);
	m_scene_info.exposure = 1.0;
	m_scene_info.shadow_map_resolution = float2(DEPTH_IMAGE_SIZE_W_H, DEPTH_IMAGE_SIZE_W_H);

	m_options.skip_skybox = false;
	m_options.skip_shadow_mapping = false;
	m_options.skip_object_rendering = false;
	m_options.skip_bloom = false;

    m_clear_stage.Init(a_arena);
    m_shadowmap_stage.Init(a_arena, a_back_buffer_count);
    m_raster_mesh_stage.Init(a_arena, a_render_target_size, a_back_buffer_count);
    m_bloom_stage.Init(a_arena);
	m_line_stage.Init(a_arena, a_back_buffer_count, LINE_MAX);

	// per frame
	m_per_frame.Init(a_arena, a_back_buffer_count);
	m_per_frame.resize(a_back_buffer_count);
	for (uint32_t i = 0; i < m_per_frame.size(); i++)
	{
		PerFrame& pfd = m_per_frame[i];
		pfd.previous_draw_area = { 0, 0 };
		pfd.scene_descriptor = AllocateDescriptor(GetSceneDescriptorLayout());

		pfd.scene_buffer.Init(BUFFER_TYPE::UNIFORM, sizeof(m_scene_info), "scene info buffer");
		DescriptorWriteBufferInfo desc_write;
		desc_write.descriptor_layout = GetSceneDescriptorLayout();
		desc_write.allocation = pfd.scene_descriptor;
		desc_write.descriptor_index = 0;

		desc_write.binding = PER_SCENE_SCENE_DATA_BINDING;
		desc_write.buffer_view = pfd.scene_buffer.GetView();
		DescriptorWriteUniformBuffer(desc_write);

		GPUBufferCreateInfo buffer_info;
		buffer_info.name = "scene STORAGE buffer";
		buffer_info.size = mbSize * 4;
		buffer_info.type = BUFFER_TYPE::STORAGE;
		buffer_info.host_writable = false;
		pfd.storage_buffer.Init(buffer_info);

		pfd.fence_value = 0;

		pfd.bloom.image = RImage();
	}
	m_render_target.format = IMAGE_FORMAT::RGBA8_SRGB;
	CreateRenderTarget(a_render_target_size);

    // IF USING RAYTRACING
    if (true)
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
}

RDescriptorLayout RenderSystem::GetSceneDescriptorLayout()
{
	static RDescriptorLayout s_scene_descriptor_layout{};
	if (s_scene_descriptor_layout.IsValid())
	{
		return s_scene_descriptor_layout;
	}

	// create a temp one just to make the function nicer.
	MemoryArena temp_arena = MemoryArenaCreate(ARENA_DEFAULT_COMMIT);

	//per-frame descriptor set 1 for renderpass
	FixedArray<DescriptorBindingInfo, 4> descriptor_bindings;
	descriptor_bindings[0].binding = PER_SCENE_SCENE_DATA_BINDING;
	descriptor_bindings[0].count = 1;
	descriptor_bindings[0].shader_stage = SHADER_STAGE::ALL;
	descriptor_bindings[0].type = DESCRIPTOR_TYPE::READONLY_CONSTANT;

	descriptor_bindings[1].binding = PER_SCENE_TRANSFORM_DATA_BINDING;
	descriptor_bindings[1].count = 1;
	descriptor_bindings[1].shader_stage = SHADER_STAGE::VERTEX;
	descriptor_bindings[1].type = DESCRIPTOR_TYPE::READONLY_BUFFER;

	descriptor_bindings[2].binding = PER_SCENE_LIGHT_DATA_BINDING;
	descriptor_bindings[2].count = 1;
	descriptor_bindings[2].shader_stage = SHADER_STAGE::FRAGMENT_PIXEL;
	descriptor_bindings[2].type = DESCRIPTOR_TYPE::READONLY_BUFFER;

	descriptor_bindings[3].binding = PER_SCENE_LIGHT_PROJECTION_VIEW_DATA_BINDING;
	descriptor_bindings[3].count = 1;
	descriptor_bindings[3].shader_stage = SHADER_STAGE::VERTEX;
	descriptor_bindings[3].type = DESCRIPTOR_TYPE::READONLY_BUFFER;
	s_scene_descriptor_layout = CreateDescriptorLayout(temp_arena, descriptor_bindings.const_slice());

	MemoryArenaFree(temp_arena);
	return s_scene_descriptor_layout;
}

void RenderSystem::StartFrame(const RCommandList a_list)
{
	PerFrame& pfd = m_per_frame[m_current_frame];
	WaitFence(m_fence, pfd.fence_value);
	pfd.fence_value = m_next_fence_value;

	PipelineBarrierImageInfo render_target_transition;
	render_target_transition.prev = IMAGE_LAYOUT::NONE;
	render_target_transition.next = IMAGE_LAYOUT::RT_COLOR;
	render_target_transition.image = m_render_target.image;
	render_target_transition.layer_count = 1;
	render_target_transition.level_count = 1;
	render_target_transition.base_array_layer = static_cast<uint16_t>(m_current_frame);
	render_target_transition.base_mip_level = 0;
	render_target_transition.image_aspect = IMAGE_ASPECT::COLOR;

	PipelineBarrierInfo pipeline_info{};
	pipeline_info.image_barriers = ConstSlice<PipelineBarrierImageInfo>(&render_target_transition, 1);
	PipelineBarriers(a_list, pipeline_info);


}

RenderSystemFrame RenderSystem::EndFrame(const RCommandList a_list, const IMAGE_LAYOUT a_current_layout)
{
	PipelineBarrierImageInfo render_target_transition;
	render_target_transition.prev = a_current_layout;
	render_target_transition.next = IMAGE_LAYOUT::RO_FRAGMENT;
	render_target_transition.image = m_render_target.image;
	render_target_transition.layer_count = 1;
	render_target_transition.level_count = 1;
	render_target_transition.base_array_layer = static_cast<uint16_t>(m_current_frame);
	render_target_transition.base_mip_level = 0;
	render_target_transition.image_aspect = IMAGE_ASPECT::COLOR;

	PipelineBarrierInfo pipeline_info{};
	pipeline_info.image_barriers = ConstSlice<PipelineBarrierImageInfo>(&render_target_transition, 1);
	PipelineBarriers(a_list, pipeline_info);

	RenderSystemFrame frame;
	frame.render_target = m_per_frame[m_current_frame].render_target_view;
	m_current_frame = (m_current_frame + 1) % m_per_frame.size();

	frame.fence = m_fence;
	frame.fence_value = m_next_fence_value++;
	return frame;
}

void RenderSystem::UpdateRenderSystem(MemoryArena& a_per_frame_arena, const RCommandList a_list, const uint2 a_draw_area, const WorldMatrixComponentPool& a_world_matrices, const RenderComponentPool& a_render_pool, const RaytraceComponentPool& a_raytrace_pool, const ConstSlice<LightComponent> a_lights)
{
	PerFrame& pfd = m_per_frame[m_current_frame];

	const ConstSlice<ECSEntity> render_entities = a_render_pool.GetEntityComponents();

	const size_t render_component_count = render_entities.size();
	DrawList draw_list;
	draw_list.draw_entries.Init(a_per_frame_arena, static_cast<uint32_t>(render_component_count));
	draw_list.transforms.Init(a_per_frame_arena, static_cast<uint32_t>(render_component_count));

	for (size_t i = 0; i < render_component_count; i++)
	{
		RenderComponent& comp = a_render_pool.GetComponent(render_entities[i]);
		const float4x4& transform = a_world_matrices.GetComponent(render_entities[i]);
		//RaytraceComponent& ray_comp = a_raytrace_pool.GetComponent(render_entities[i]);

		if (comp.material_dirty)
		{
			const uint64_t upload_offset = m_upload_allocator.AllocateUploadMemory(sizeof(comp.material_data), pfd.fence_value);
			m_upload_allocator.MemcpyIntoBuffer(upload_offset, &comp.material_data, sizeof(comp.material_data));
			Material::WriteMaterial(comp.material, a_list, m_upload_allocator.GetBuffer(), upload_offset);
			comp.material_dirty = false;
		}

		// raytrace stuff
		//if (ray_comp.needs_build)
		//{
		//	GPUBufferView view;
		//	bool success = m_raytrace_data.acceleration_structure_buffer.Allocate(ray_comp.build_size, view);
		//	BB_ASSERT(success, "failed to allocate acceleration structure data");
		//	ray_comp.acceleration_structure = CreateBottomLevelAccelerationStruct(ray_comp.build_size, view.buffer, view.offset);
		//	ray_comp.acceleration_struct_address = GetAccelerationStructureAddress(ray_comp.acceleration_structure);
		//	ray_comp.acceleration_buffer_offset = view.offset;
		//	ray_comp.needs_build = false;

		//	AccelerationStructGeometrySize geometry_size;
		//	geometry_size.vertex_count = comp.index_count * 3;
		//	geometry_size.vertex_stride = sizeof(float3);
		//	geometry_size.transform_address = 0; // TODO
		//	geometry_size.index_offset = comp.index_start;
		//	geometry_size.vertex_offset = comp.mesh.vertex_position_offset;

		//	const uint32_t primitive_count = comp.index_count / 3;

		//	BuildBottomLevelAccelerationStructInfo acc_build_info;
		//	acc_build_info.acc_struct = ray_comp.acceleration_structure;
		//	acc_build_info.scratch_buffer_address = 0; // TODO
		//	acc_build_info.geometry_sizes = ConstSlice<AccelerationStructGeometrySize>(&geometry_size, 1);
		//	acc_build_info.primitive_counts = ConstSlice<uint32_t>(&primitive_count, 1);
		//	//BuildBottomLevelAccelerationStruct(a_per_frame_arena, a_list, acc_build_info);
		//}

		DrawList::DrawEntry entry;
		entry.mesh = comp.mesh;
		entry.master_material = comp.master_material;
		entry.material = comp.material;
		entry.index_start = comp.index_start;
		entry.index_count = comp.index_count;

		ShaderTransform shader_transform;
		shader_transform.transform = transform;
		shader_transform.inverse = Float4x4Inverse(transform);

		draw_list.draw_entries.push_back(entry);
		draw_list.transforms.push_back(shader_transform);
	}

	//if (m_raytrace_data.top_level.must_rebuild || !m_raytrace_data.top_level.accel_struct.IsValid())
    if (false)
	{
		StaticArray<AccelerationStructureInstanceInfo> instances{};
		instances.Init(a_per_frame_arena, static_cast<uint32_t>(render_component_count), static_cast<uint32_t>(render_component_count));
		for (size_t i = 0; i < render_component_count; i++)
		{
			instances[i].transform = &draw_list.transforms[i].transform;
			instances[i].shader_custom_index = 0;
			instances[i].mask = 0xFF;
			instances[i].shader_binding_table_offset = 0;
			instances[i].acceleration_structure_address = a_raytrace_pool.GetComponent(render_entities[i]).acceleration_struct_address;
		}

		BuildTopLevelAccelerationStructure(a_per_frame_arena, a_list, instances.const_slice());
	}

	BindIndexBuffer(a_list, 0);
	UpdateConstantBuffer(m_current_frame, a_list, a_draw_area, a_lights);

    // sam please find a better way
    const RPipelineLayout pipe_layout = Material::BindMaterial(a_list, draw_list.draw_entries[0].master_material);
    {
        const uint32_t buffer_indices[] = { 0, 0 };
        const DescriptorAllocation& global_desc_alloc = GetGlobalDescriptorAllocation();
        const size_t buffer_offsets[]{ global_desc_alloc.offset, pfd.scene_descriptor.offset };
        //set 1-2
        SetDescriptorBufferOffset(a_list,
            pipe_layout,
            SPACE_GLOBAL,
            _countof(buffer_offsets),
            buffer_indices,
            buffer_offsets);
    }
	
    m_clear_stage.ExecutePass(a_list, a_draw_area, GetImageView(pfd.render_target_view));

	ResourceUploadPass(pfd, a_list, draw_list, a_lights);

    m_shadowmap_stage.ExecutePass(a_list, m_current_frame, uint2(DEPTH_IMAGE_SIZE_W_H, DEPTH_IMAGE_SIZE_W_H), draw_list, a_lights);
    m_raster_mesh_stage.ExecutePass(a_list, m_current_frame, a_draw_area, draw_list, GetImageView(pfd.render_target_view), GetImageView(pfd.bloom.descriptor_index_0));
    if (!m_options.skip_bloom)
	    m_bloom_stage.ExecutePass(a_list, pfd.bloom.resolution, pfd.bloom.image, pfd.bloom.descriptor_index_0, pfd.bloom.descriptor_index_1, a_draw_area, GetImageView(pfd.render_target_view));
}

void RenderSystem::Resize(const uint2 a_new_extent, const bool a_force)
{
	if (m_render_target.extent == a_new_extent && !a_force)
		return;

	// wait until the rendering is all done
	WaitFence(m_fence, m_next_fence_value - 1);
	GPUWaitIdle();

	FreeImage(m_render_target.image);
	for (uint32_t i = 0; i < m_per_frame.size(); i++)
	{
		FreeImageView(m_per_frame[i].render_target_view);
	}
	CreateRenderTarget(a_new_extent);
}

void RenderSystem::ResizeNewFormat(const uint2 a_render_target_size, const IMAGE_FORMAT a_render_target_format)
{
	m_render_target.format = a_render_target_format;
	Resize(a_render_target_size, true);
}

void RenderSystem::Screenshot(const PathString& a_path) const
{
	// wait until the rendering is all done
	WaitFence(m_fence, m_next_fence_value - 1);

	const uint32_t prev_frame = (m_current_frame + 1) % m_per_frame.size();

	ImageInfo image_info;
	image_info.image = m_render_target.image;
	image_info.extent = m_render_target.extent;
	image_info.offset = int2(0);
	image_info.array_layers = 1;
	image_info.base_array_layer = static_cast<uint16_t>(prev_frame);
	image_info.mip_level = 0;

	const bool success = Asset::ReadWriteTextureDeferred(a_path.GetView(), image_info);
	BB_ASSERT(success, "failed to write screenshot image to disk");
}

void RenderSystem::SetView(const float4x4& a_view, const float3& a_view_position)
{
	m_scene_info.view = a_view;
	m_scene_info.view_pos = float3(a_view_position.x, a_view_position.y, a_view_position.z);
}

void RenderSystem::SetProjection(const float4x4& a_projection)
{
	m_scene_info.proj = a_projection;
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

void RenderSystem::UpdateConstantBuffer(const uint32_t a_frame_index, const RCommandList a_list, const uint2 a_draw_area_size, const ConstSlice<LightComponent> a_lights)
{
    // temp jank
    PerFrame& a_pfd = m_per_frame[a_frame_index];
    m_clear_stage.UpdateConstantBuffer(m_scene_info);
    m_shadowmap_stage.UpdateConstantBuffer(a_frame_index, m_scene_info);
	m_scene_info.light_count = static_cast<uint32_t>(a_lights.size());
	m_scene_info.scene_resolution = a_draw_area_size;

	if (a_pfd.previous_draw_area != a_draw_area_size)
	{
		if (a_pfd.bloom.image.IsValid())
		{
			FreeImage(a_pfd.bloom.image);
			FreeImageView(a_pfd.bloom.descriptor_index_0);
			FreeImageView(a_pfd.bloom.descriptor_index_1);
		}

		{
			const float2 downscaled_bloom_image = {
				static_cast<float>(a_draw_area_size.x) * BLOOM_IMAGE_DOWNSCALE_FACTOR,
				static_cast<float>(a_draw_area_size.y) * BLOOM_IMAGE_DOWNSCALE_FACTOR
			};

			a_pfd.bloom.resolution = uint2(static_cast<uint32_t>(downscaled_bloom_image.x), static_cast<uint32_t>(downscaled_bloom_image.y));

			ImageCreateInfo bloom_img_info;
			bloom_img_info.name = "bloom image";
			bloom_img_info.width = a_pfd.bloom.resolution.x;
			bloom_img_info.height = a_pfd.bloom.resolution.y;
			bloom_img_info.depth = 1;
			bloom_img_info.mip_levels = 1;
			bloom_img_info.array_layers = 2;			// 0 == bloom image, 1 = bloom vertical blur
			bloom_img_info.format = m_render_target.format;
			bloom_img_info.usage = IMAGE_USAGE::RENDER_TARGET;
			bloom_img_info.type = IMAGE_TYPE::TYPE_2D;
			bloom_img_info.use_optimal_tiling = true;
			bloom_img_info.is_cube_map = false;
			a_pfd.bloom.image = CreateImage(bloom_img_info);

			ImageViewCreateInfo bloom_img_view_info;
			bloom_img_view_info.name = "bloom image view";
			bloom_img_view_info.image = a_pfd.bloom.image;
			bloom_img_view_info.type = IMAGE_VIEW_TYPE::TYPE_2D;
			bloom_img_view_info.base_array_layer = 0;
			bloom_img_view_info.array_layers = 1;
			bloom_img_view_info.mip_levels = 1;
			bloom_img_view_info.base_mip_level = 0;
			bloom_img_view_info.format = m_render_target.format;
			bloom_img_view_info.aspects = IMAGE_ASPECT::COLOR;
			a_pfd.bloom.descriptor_index_0 = CreateImageView(bloom_img_view_info);

			bloom_img_view_info.base_array_layer = 1;
			a_pfd.bloom.descriptor_index_1 = CreateImageView(bloom_img_view_info);

			FixedArray<PipelineBarrierImageInfo, 2> bloom_initial_stages;
			bloom_initial_stages[0].prev = IMAGE_LAYOUT::NONE;
			bloom_initial_stages[0].next = IMAGE_LAYOUT::RT_COLOR;
			bloom_initial_stages[0].image = a_pfd.bloom.image;
			bloom_initial_stages[0].layer_count = 1;
			bloom_initial_stages[0].level_count = 1;
			bloom_initial_stages[0].base_array_layer = 0;
			bloom_initial_stages[0].base_mip_level = 0;
			bloom_initial_stages[0].image_aspect = IMAGE_ASPECT::COLOR;

			bloom_initial_stages[1].prev = IMAGE_LAYOUT::NONE;
			bloom_initial_stages[1].next = IMAGE_LAYOUT::RO_FRAGMENT;
			bloom_initial_stages[1].image = a_pfd.bloom.image;
			bloom_initial_stages[1].layer_count = 1;
			bloom_initial_stages[1].level_count = 1;
			bloom_initial_stages[1].base_array_layer = 1;
			bloom_initial_stages[1].base_mip_level = 0;
			bloom_initial_stages[1].image_aspect = IMAGE_ASPECT::COLOR;

			PipelineBarrierInfo barriers{};
			barriers.image_barriers = bloom_initial_stages.const_slice();
			PipelineBarriers(a_list, barriers);
		}

		a_pfd.previous_draw_area = a_draw_area_size;
	}
}

void RenderSystem::ResourceUploadPass(PerFrame& a_pfd, const RCommandList a_list, const DrawList& a_draw_list, const ConstSlice<LightComponent> a_lights)
{
	GPULinearBuffer& cur_scene_buffer = a_pfd.storage_buffer;
	cur_scene_buffer.Clear();

	{	// write scene info
		a_pfd.scene_buffer.WriteTo(&m_scene_info, sizeof(m_scene_info), 0);
	}

	const size_t matrices_upload_size = a_draw_list.draw_entries.size() * sizeof(ShaderTransform);
	const size_t light_upload_size = a_lights.size() * sizeof(Light);
	const size_t light_projection_view_size = a_lights.size() * sizeof(float4x4);

	auto memcpy_and_advance = [](const GPUUploadRingAllocator& a_buffer, const size_t a_dst_offset, const void* a_src_data, const size_t a_src_size)
		{
			const bool success = a_buffer.MemcpyIntoBuffer(a_dst_offset, a_src_data, a_src_size);
			BB_ASSERT(success, "failed to memcpy mesh data into gpu visible buffer");
			return a_dst_offset + a_src_size;
		};

	// optimize this
	const size_t total_size = matrices_upload_size + light_upload_size + light_projection_view_size;

	uint64_t upload_offset = m_upload_allocator.AllocateUploadMemory(total_size, a_pfd.fence_value);
	BB_ASSERT(upload_offset != uint64_t(-1), "upload offset invalid");

	const uint64_t matrix_offset = upload_offset;
	upload_offset = memcpy_and_advance(m_upload_allocator, upload_offset, a_draw_list.transforms.data(), matrices_upload_size);

	const uint64_t light_offset = upload_offset;
	for (uint32_t i = 0; i < a_lights.size(); i++)
		upload_offset = memcpy_and_advance(m_upload_allocator, upload_offset, &a_lights[i].light, sizeof(Light));

	const uint64_t light_projection_view_offset = upload_offset;
	for (uint32_t i = 0; i < a_lights.size(); i++)
		upload_offset = memcpy_and_advance(m_upload_allocator, upload_offset, &a_lights[i].projection_view, sizeof(float4x4));

	GPUBufferView transform_view;
	bool success = cur_scene_buffer.Allocate(matrices_upload_size, transform_view);
	BB_ASSERT(success, "failed to allocate frame memory");
	GPUBufferView light_view;
	success = cur_scene_buffer.Allocate(light_upload_size, light_view);
	BB_ASSERT(success, "failed to allocate frame memory");
	GPUBufferView light_projection_view;
	success = cur_scene_buffer.Allocate(light_projection_view_size, light_projection_view);
	BB_ASSERT(success, "failed to allocate frame memory");

	//upload to some GPU buffer here.
	RenderCopyBuffer matrix_buffer_copy;
	matrix_buffer_copy.src = m_upload_allocator.GetBuffer();
	matrix_buffer_copy.dst = cur_scene_buffer.GetBuffer();
	size_t copy_region_count = 0;
	FixedArray<RenderCopyBufferRegion, 3> buffer_regions; //0 = matrix, 1 = lights, 2 = light projection view
	if (matrices_upload_size)
	{
		buffer_regions[copy_region_count].src_offset = matrix_offset;
		buffer_regions[copy_region_count].dst_offset = transform_view.offset;
		buffer_regions[copy_region_count].size = matrices_upload_size;
		++copy_region_count;
	}

	if (light_upload_size)
	{
		buffer_regions[copy_region_count].src_offset = light_offset;
		buffer_regions[copy_region_count].dst_offset = light_view.offset;
		buffer_regions[copy_region_count].size = light_upload_size;
		++copy_region_count;

		buffer_regions[copy_region_count].src_offset = light_projection_view_offset;
		buffer_regions[copy_region_count].dst_offset = light_projection_view.offset;
		buffer_regions[copy_region_count].size = light_projection_view_size;
		++copy_region_count;
	}

	matrix_buffer_copy.regions = buffer_regions.slice(copy_region_count);
	if (copy_region_count)
	{
		CopyBuffer(a_list, matrix_buffer_copy);

		// WRITE DESCRIPTORS HERE
		DescriptorWriteBufferInfo desc_write;
		desc_write.descriptor_layout = GetSceneDescriptorLayout();
		desc_write.allocation = a_pfd.scene_descriptor;
		desc_write.descriptor_index = 0;

		if (matrices_upload_size)
		{
			desc_write.binding = PER_SCENE_TRANSFORM_DATA_BINDING;
			desc_write.buffer_view = transform_view;
			DescriptorWriteStorageBuffer(desc_write);
		}
		if (light_upload_size)
		{
			desc_write.binding = PER_SCENE_LIGHT_DATA_BINDING;
			desc_write.buffer_view = light_view;
			DescriptorWriteStorageBuffer(desc_write);

			desc_write.binding = PER_SCENE_LIGHT_PROJECTION_VIEW_DATA_BINDING;
			desc_write.buffer_view = light_projection_view;
			DescriptorWriteStorageBuffer(desc_write);
		}
	}
}

void RenderSystem::CreateRenderTarget(const uint2 a_render_target_size)
{
	ImageCreateInfo render_target_create;
	render_target_create.name = "render target color";
	render_target_create.width = a_render_target_size.x;
	render_target_create.height = a_render_target_size.y;
	render_target_create.depth = 1;
	render_target_create.array_layers = static_cast<uint16_t>(m_per_frame.size());
	render_target_create.mip_levels = 1;
	render_target_create.type = IMAGE_TYPE::TYPE_2D;
	render_target_create.format = m_render_target.format;
	render_target_create.usage = IMAGE_USAGE::RENDER_TARGET;
	render_target_create.use_optimal_tiling = true;
	render_target_create.is_cube_map = false;
	m_render_target.image = CreateImage(render_target_create);
	m_render_target.extent = a_render_target_size;

	ImageViewCreateInfo image_view_create;
	image_view_create.name = "render target color view";
	image_view_create.array_layers = 1;
	image_view_create.mip_levels = 1;
	image_view_create.base_mip_level = 0;
	image_view_create.image = m_render_target.image;
	image_view_create.format = m_render_target.format;
	image_view_create.type = IMAGE_VIEW_TYPE::TYPE_2D;
	image_view_create.aspects = IMAGE_ASPECT::COLOR;

	for (uint32_t i = 0; i < m_per_frame.size(); i++)
	{
		// render target
		image_view_create.base_array_layer = static_cast<uint16_t>(i);
		m_per_frame[i].render_target_view = CreateImageView(image_view_create);
	}
}
