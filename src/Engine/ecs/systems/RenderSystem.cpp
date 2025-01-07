#include "RenderSystem.hpp"
#include "MaterialSystem.hpp"
#include "Math.inl"
#include "Renderer.hpp"

using namespace BB;

constexpr float BLOOM_IMAGE_DOWNSCALE_FACTOR = 1.f;

void RenderSystem::Init(MemoryArena& a_arena, const uint32_t a_back_buffer_count, const uint32_t a_max_lights)
{
	m_per_frame.Init(a_arena, a_back_buffer_count);
	
	m_global_buffer.light_max = a_max_lights;
	GPUBufferCreateInfo buff_create;
	buff_create.name = "light buffer";
	buff_create.size = a_max_lights * (sizeof(Light) + sizeof(float4x4));
	buff_create.type = BUFFER_TYPE::UNIFORM;
	buff_create.host_writable = false;
	m_global_buffer.buffer.Init(buff_create);

	m_global_buffer.buffer.Allocate(a_max_lights * sizeof(Light), m_global_buffer.light_view);
	m_global_buffer.buffer.Allocate(a_max_lights * sizeof(float4x4), m_global_buffer.light_viewproj_view);
}

void RenderSystem::UpdateLights(MemoryArena& a_temp_arena, const RCommandList a_list, const LightComponentPool& a_light_pool, const ConstSlice<ECSEntity> a_update_lights)
{
	const size_t buffer_size = a_update_lights.size() * (sizeof(Light) + sizeof(float4x4));
	UploadBuffer upload_buffer = m_upload_allocator.AllocateUploadMemory(buffer_size, m_last_completed_fence_value);

	size_t offset = 0;
	for (size_t i = 0; i < a_update_lights.size(); i++)
	{
		upload_buffer.SafeMemcpy(offset, &a_light_pool.GetComponent(i).light, sizeof(Light));
		offset += sizeof(Light);
	}

	for (size_t i = 0; i < a_update_lights.size(); i++)
	{
		upload_buffer.SafeMemcpy(offset, &a_light_pool.GetComponent(i).projection_view, sizeof(float4x4));
		offset += sizeof(float4x4);
	}

	StaticArray<RenderCopyBufferRegion> copy_regions{};
	copy_regions.Init(a_temp_arena, static_cast<uint32_t>(a_update_lights.size() * 2));

}

void RenderSystem::UpdateRenderSystem(MemoryArena& a_arena, const RCommandList a_list, const WorldMatrixComponentPool& a_world_matrices, const RenderComponentPool& a_render_pool)
{
	PerFrame& pfd = m_per_frame[m_current_backbuffer];
	WaitFence(m_fence, pfd.fence_value);
	pfd.fence_value = m_next_fence_value;

	const ConstSlice<ECSEntity> render_entities = a_render_pool.GetEntityComponents();

	const size_t render_component_count = render_entities.size();
	DrawList draw_list;
	draw_list.draw_entries.Init(a_arena, static_cast<uint32_t>(render_component_count));
	draw_list.transforms.Init(a_arena, static_cast<uint32_t>(render_component_count));

	for (size_t i = 0; i < render_component_count; i++)
	{
		RenderComponent& comp = a_render_pool.GetComponent(render_entities[i]);
		const float4x4& transform = a_world_matrices.GetComponent(render_entities[i]);

		if (comp.material_dirty)
		{
			const UploadBuffer upload = m_upload_allocator.AllocateUploadMemory(sizeof(comp.material_data), pfd.fence_value);
			upload.SafeMemcpy(0, &comp.material_data, sizeof(comp.material_data));
			Material::WriteMaterial(comp.material, a_list, upload.buffer, upload.base_offset);
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

		draw_list.draw_entries.push_back(entry);
		draw_list.transforms.push_back(shader_transform);
	}

	m_current_backbuffer = ;
}

void RenderSystem::UpdateConstantBuffer(PerFrame& a_pfd, const RCommandList a_list, const uint2 a_draw_area_size)
{
	m_scene_info.light_count = m_light_pool.GetSize();
	m_scene_info.scene_resolution = a_draw_area_size;
	m_scene_info.shadow_map_count = a_pfd.shadow_map.render_pass_views.size();
	m_scene_info.shadow_map_array_descriptor = a_pfd.shadow_map.descriptor_index;
	m_scene_info.skybox_texture = m_skybox_descriptor_index;

	if (a_pfd.previous_draw_area != a_draw_area_size)
	{
		if (a_pfd.depth_image.IsValid())
		{
			FreeImage(a_pfd.depth_image);
			FreeImageViewShaderInaccessible(a_pfd.depth_image_view);
		}
		{
			ImageCreateInfo depth_img_info;
			depth_img_info.name = "scene depth buffer";
			depth_img_info.width = a_draw_area_size.x;
			depth_img_info.height = a_draw_area_size.y;
			depth_img_info.depth = 1;
			depth_img_info.mip_levels = 1;
			depth_img_info.array_layers = 1;
			depth_img_info.format = IMAGE_FORMAT::D24_UNORM_S8_UINT;
			depth_img_info.usage = IMAGE_USAGE::DEPTH;
			depth_img_info.type = IMAGE_TYPE::TYPE_2D;
			depth_img_info.use_optimal_tiling = true;
			depth_img_info.is_cube_map = false;
			a_pfd.depth_image = CreateImage(depth_img_info);

			ImageViewCreateInfo depth_img_view_info;
			depth_img_view_info.name = "scene depth view";
			depth_img_view_info.image = a_pfd.depth_image;
			depth_img_view_info.type = IMAGE_VIEW_TYPE::TYPE_2D;
			depth_img_view_info.base_array_layer = 0;
			depth_img_view_info.array_layers = 1;
			depth_img_view_info.mip_levels = 1;
			depth_img_view_info.base_mip_level = 0;
			depth_img_view_info.format = IMAGE_FORMAT::D24_UNORM_S8_UINT;
			depth_img_view_info.aspects = IMAGE_ASPECT::DEPTH_STENCIL;
			a_pfd.depth_image_view = CreateImageViewShaderInaccessible(depth_img_view_info);
		}

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
			bloom_img_info.format = RENDER_TARGET_IMAGE_FORMAT;
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
			bloom_img_view_info.format = RENDER_TARGET_IMAGE_FORMAT;
			bloom_img_view_info.aspects = IMAGE_ASPECT::COLOR;
			a_pfd.bloom.descriptor_index_0 = CreateImageView(bloom_img_view_info);

			bloom_img_view_info.base_array_layer = 1;
			a_pfd.bloom.descriptor_index_1 = CreateImageView(bloom_img_view_info);

			FixedArray<PipelineBarrierImageInfo, 2> bloom_initial_stages;
			bloom_initial_stages[0].src_mask = BARRIER_ACCESS_MASK::NONE;
			bloom_initial_stages[0].dst_mask = BARRIER_ACCESS_MASK::COLOR_ATTACHMENT_WRITE;
			bloom_initial_stages[0].image = a_pfd.bloom.image;
			bloom_initial_stages[0].old_layout = IMAGE_LAYOUT::UNDEFINED;
			bloom_initial_stages[0].new_layout = IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL;
			bloom_initial_stages[0].layer_count = 1;
			bloom_initial_stages[0].level_count = 1;
			bloom_initial_stages[0].base_array_layer = 0;
			bloom_initial_stages[0].base_mip_level = 0;
			bloom_initial_stages[0].src_stage = BARRIER_PIPELINE_STAGE::TOP_OF_PIPELINE;
			bloom_initial_stages[0].dst_stage = BARRIER_PIPELINE_STAGE::COLOR_ATTACH_OUTPUT;
			bloom_initial_stages[0].aspects = IMAGE_ASPECT::COLOR;

			bloom_initial_stages[1].src_mask = BARRIER_ACCESS_MASK::NONE;
			bloom_initial_stages[1].dst_mask = BARRIER_ACCESS_MASK::SHADER_READ;
			bloom_initial_stages[1].image = a_pfd.bloom.image;
			bloom_initial_stages[1].old_layout = IMAGE_LAYOUT::UNDEFINED;
			bloom_initial_stages[1].new_layout = IMAGE_LAYOUT::SHADER_READ_ONLY;
			bloom_initial_stages[1].layer_count = 1;
			bloom_initial_stages[1].level_count = 1;
			bloom_initial_stages[1].base_array_layer = 1;
			bloom_initial_stages[1].base_mip_level = 0;
			bloom_initial_stages[1].src_stage = BARRIER_PIPELINE_STAGE::TOP_OF_PIPELINE;
			bloom_initial_stages[1].dst_stage = BARRIER_PIPELINE_STAGE::FRAGMENT_SHADER;
			bloom_initial_stages[1].aspects = IMAGE_ASPECT::COLOR;

			PipelineBarrierInfo barriers{};
			barriers.image_infos = bloom_initial_stages.data();
			barriers.image_info_count = bloom_initial_stages.size();
			PipelineBarriers(a_list, barriers);
		}

		a_pfd.previous_draw_area = a_draw_area_size;
	}
}

void RenderSystem::SkyboxPass(const PerFrame& a_pfd, const RCommandList a_list, const RImageView a_render_target_view, const uint2 a_draw_area_size, const int2 a_draw_area_offset)
{
	const RPipelineLayout pipe_layout = BindShaders(a_list, Material::GetMaterialShaders(m_skybox_material));
	{
		const uint32_t buffer_indices[] = { 0, 0 };
		const DescriptorAllocation& global_desc_alloc = GetGlobalDescriptorAllocation();
		const size_t buffer_offsets[]{ global_desc_alloc.offset, a_pfd.scene_descriptor.offset };
		//set 1-2
		SetDescriptorBufferOffset(a_list,
			pipe_layout,
			SPACE_GLOBAL,
			_countof(buffer_offsets),
			buffer_indices,
			buffer_offsets);
	}

	RenderingAttachmentColor color_attach;
	color_attach.load_color = false;
	color_attach.store_color = true;
	color_attach.image_layout = IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL;
	color_attach.image_view = a_render_target_view;

	StartRenderingInfo start_rendering_info;
	start_rendering_info.render_area_extent = a_draw_area_size;
	start_rendering_info.render_area_offset = a_draw_area_offset;
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
	if (m_options.skip_skybox)
	{
		EndRenderPass(a_list);
		return;
	}

	SetFrontFace(a_list, false);
	SetCullMode(a_list, CULL_MODE::NONE);

	DrawCubemap(a_list, 1, 0);

	EndRenderPass(a_list);
}

void RenderSystem::ResourceUploadPass(PerFrame& a_pfd, const RCommandList a_list, const DrawList& a_draw_list)
{
	GPULinearBuffer& cur_scene_buffer = a_pfd.storage_buffer;
	cur_scene_buffer.Clear();

	{	// write scene info
		a_pfd.scene_buffer.WriteTo(&m_scene_info, sizeof(m_scene_info), 0);
	}

	const size_t matrices_upload_size = a_draw_list.draw_entries.size() * sizeof(ShaderTransform);
	// optimize this
	const size_t total_size = matrices_upload_size + light_upload_size + light_projection_view_size;

	const UploadBuffer upload_buffer = m_upload_allocator.AllocateUploadMemory(total_size, a_pfd.fence_value);

	size_t bytes_uploaded = 0;

	upload_buffer.SafeMemcpy(bytes_uploaded, a_draw_list.transforms.data(), matrices_upload_size);
	const size_t matrix_offset = bytes_uploaded + upload_buffer.base_offset;
	bytes_uploaded += matrices_upload_size;

	for (uint32_t i = 0; i < m_light_pool.GetSize(); i++)
		upload_buffer.SafeMemcpy(bytes_uploaded, &m_light_pool.GetComponent(i).light, sizeof(Light));

	const size_t light_offset = bytes_uploaded + upload_buffer.base_offset;
	bytes_uploaded += light_upload_size;

	for (uint32_t i = 0; i < m_light_pool.GetSize(); i++)
		upload_buffer.SafeMemcpy(bytes_uploaded, &m_light_pool.GetComponent(i).projection_view, sizeof(float4x4));
	const size_t light_projection_view_offset = bytes_uploaded + upload_buffer.base_offset;
	bytes_uploaded += light_projection_view_size;

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
	matrix_buffer_copy.src = upload_buffer.buffer;
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

void RenderSystem::ShadowMapPass(const PerFrame& a_pfd, const RCommandList a_list, const uint2 a_shadow_map_resolution, const DrawList& a_draw_list)
{
	const uint32_t shadow_map_count = m_light_pool.GetSize();
	if (shadow_map_count == 0)
	{
		return;
	}
	BB_ASSERT(shadow_map_count <= a_pfd.shadow_map.render_pass_views.size(), "too many lights! Make a dynamic shadow mapping array");

	const RPipelineLayout pipe_layout = BindShaders(a_list, Material::GetMaterialShaders(m_shadowmap_material));

	PipelineBarrierImageInfo shadow_map_write_transition = {};
	shadow_map_write_transition.src_mask = BARRIER_ACCESS_MASK::NONE;
	shadow_map_write_transition.dst_mask = BARRIER_ACCESS_MASK::DEPTH_STENCIL_READ_WRITE;
	shadow_map_write_transition.image = a_pfd.shadow_map.image;
	shadow_map_write_transition.old_layout = IMAGE_LAYOUT::UNDEFINED;
	shadow_map_write_transition.new_layout = IMAGE_LAYOUT::DEPTH_STENCIL_ATTACHMENT;
	shadow_map_write_transition.layer_count = shadow_map_count;
	shadow_map_write_transition.level_count = 1;
	shadow_map_write_transition.base_array_layer = 0;
	shadow_map_write_transition.base_mip_level = 0;
	shadow_map_write_transition.src_stage = BARRIER_PIPELINE_STAGE::TOP_OF_PIPELINE;
	shadow_map_write_transition.dst_stage = BARRIER_PIPELINE_STAGE::FRAGMENT_TEST;
	shadow_map_write_transition.aspects = IMAGE_ASPECT::DEPTH;

	PipelineBarrierInfo pipeline_info{};
	pipeline_info.image_info_count = 1;
	pipeline_info.image_infos = &shadow_map_write_transition;
	PipelineBarriers(a_list, pipeline_info);

	RenderingAttachmentDepth depth_attach{};
	depth_attach.load_depth = false;
	depth_attach.store_depth = true;
	depth_attach.image_layout = IMAGE_LAYOUT::DEPTH_STENCIL_ATTACHMENT;

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

	if (m_options.skip_shadow_mapping)
	{
		// validation issues, not nice to use but works for now.
		ClearDepthImageInfo depth_clear;
		depth_clear.image = a_pfd.shadow_map.image;
		depth_clear.clear_depth = 1;
		depth_clear.layout = IMAGE_LAYOUT::DEPTH_STENCIL_ATTACHMENT;
		depth_clear.depth_aspects = IMAGE_ASPECT::DEPTH;
		depth_clear.layer_count = shadow_map_count;
		depth_clear.base_array_layer = 0;
		depth_clear.level_count = 1;
		depth_clear.base_mip_level = 0;

		ClearDepthImage(a_list, depth_clear);
	}
	else
	{
		for (uint32_t shadow_map_index = 0; shadow_map_index < shadow_map_count; shadow_map_index++)
		{
			depth_attach.image_view = a_pfd.shadow_map.render_pass_views[shadow_map_index];

			StartRenderPass(a_list, rendering_info);

			if (!m_options.skip_shadow_mapping)
			{
				for (uint32_t draw_index = 0; draw_index < a_draw_list.draw_entries.size(); draw_index++)
				{
					const DrawList::DrawEntry& mesh_draw_call = a_draw_list.draw_entries[draw_index];

					ShaderIndicesShadowMapping shader_indices;
					shader_indices.vertex_buffer_offset = static_cast<uint32_t>(mesh_draw_call.mesh.vertex_buffer_offset);
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
			}

			EndRenderPass(a_list);
		}
	}

	PipelineBarrierImageInfo shadow_map_read_transition = {};
	shadow_map_read_transition.src_mask = BARRIER_ACCESS_MASK::DEPTH_STENCIL_READ_WRITE;
	shadow_map_read_transition.dst_mask = BARRIER_ACCESS_MASK::SHADER_READ;
	shadow_map_read_transition.image = a_pfd.shadow_map.image;
	shadow_map_read_transition.old_layout = IMAGE_LAYOUT::DEPTH_STENCIL_ATTACHMENT;
	shadow_map_read_transition.new_layout = IMAGE_LAYOUT::DEPTH_STENCIL_READ_ONLY;
	shadow_map_read_transition.layer_count = shadow_map_count;
	shadow_map_read_transition.level_count = 1;
	shadow_map_read_transition.base_array_layer = 0;
	shadow_map_read_transition.base_mip_level = 0;
	shadow_map_read_transition.src_stage = BARRIER_PIPELINE_STAGE::FRAGMENT_TEST;
	shadow_map_read_transition.dst_stage = BARRIER_PIPELINE_STAGE::FRAGMENT_SHADER;
	shadow_map_read_transition.aspects = IMAGE_ASPECT::DEPTH;

	pipeline_info = {};
	pipeline_info.image_info_count = 1;
	pipeline_info.image_infos = &shadow_map_read_transition;
	PipelineBarriers(a_list, pipeline_info);
}

void RenderSystem::GeometryPass(const PerFrame& a_pfd, const RCommandList a_list, const RImageView a_render_target_view, const uint2 a_draw_area_size, const int2 a_draw_area_offset, const DrawList& a_draw_list)
{
	PipelineBarrierImageInfo image_transitions[1]{};
	image_transitions[0].src_mask = BARRIER_ACCESS_MASK::NONE;
	image_transitions[0].dst_mask = BARRIER_ACCESS_MASK::DEPTH_STENCIL_READ_WRITE;
	image_transitions[0].image = a_pfd.depth_image;
	image_transitions[0].old_layout = IMAGE_LAYOUT::UNDEFINED;
	image_transitions[0].new_layout = IMAGE_LAYOUT::DEPTH_STENCIL_ATTACHMENT;
	image_transitions[0].layer_count = 1;
	image_transitions[0].level_count = 1;
	image_transitions[0].base_array_layer = 0;
	image_transitions[0].base_mip_level = 0;
	image_transitions[0].src_stage = BARRIER_PIPELINE_STAGE::FRAGMENT_TEST;
	image_transitions[0].dst_stage = BARRIER_PIPELINE_STAGE::FRAGMENT_TEST;
	image_transitions[0].aspects = IMAGE_ASPECT::DEPTH_STENCIL;

	PipelineBarrierInfo pipeline_info{};
	pipeline_info.image_info_count = _countof(image_transitions);
	pipeline_info.image_infos = image_transitions;
	PipelineBarriers(a_list, pipeline_info);

	FixedArray<RenderingAttachmentColor, 2> color_attachs;
	color_attachs[0].load_color = true;
	color_attachs[0].store_color = true;
	color_attachs[0].image_layout = IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL;
	color_attachs[0].image_view = a_render_target_view;

	color_attachs[1].load_color = false;
	color_attachs[1].store_color = true;
	color_attachs[1].image_layout = IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL;
	color_attachs[1].image_view = GetImageView(a_pfd.bloom.descriptor_index_0);
	const uint32_t color_attach_count = 2;

	RenderingAttachmentDepth depth_attach{};
	depth_attach.load_depth = false;
	depth_attach.store_depth = true;
	depth_attach.image_layout = IMAGE_LAYOUT::DEPTH_STENCIL_ATTACHMENT;
	depth_attach.image_view = a_pfd.depth_image_view;

	StartRenderingInfo rendering_info;
	rendering_info.color_attachments = color_attachs.slice(color_attach_count);
	rendering_info.depth_attachment = &depth_attach;
	rendering_info.render_area_extent = a_draw_area_size;
	rendering_info.render_area_offset = a_draw_area_offset;

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

	if (m_options.skip_object_rendering)
	{
		EndRenderPass(a_list);
		return;
	}
	SetFrontFace(a_list, false);
	SetCullMode(a_list, CULL_MODE::BACK);

	for (uint32_t i = 0; i < a_draw_list.draw_entries.size(); i++)
	{
		const DrawList::DrawEntry& mesh_draw_call = a_draw_list.draw_entries[i];

		const ConstSlice<ShaderEffectHandle> shader_effects = Material::GetMaterialShaders(mesh_draw_call.master_material);
		const RPipelineLayout pipe_layout = BindShaders(a_list, shader_effects);
		{
			const uint32_t buffer_indices[] = { 0 };
			const size_t buffer_offsets[]{ Material::GetMaterialDescAllocation().offset };
			//set 3
			SetDescriptorBufferOffset(a_list,
				pipe_layout,
				SPACE_PER_MATERIAL,
				_countof(buffer_offsets),
				buffer_indices,
				buffer_offsets);
		}

		ShaderIndices shader_indices;
		shader_indices.transform_index = i;
		shader_indices.vertex_buffer_offset = static_cast<uint32_t>(mesh_draw_call.mesh.vertex_buffer_offset);
		shader_indices.material_index = RDescriptorIndex(mesh_draw_call.material.index);
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

void RenderSystem::BloomPass(const PerFrame& a_pfd, const RCommandList a_list, const RImageView a_render_target, const uint2 a_draw_area_size, const int2 a_draw_area_offset)
{
	if (m_options.skip_bloom)
		return;

	Slice<const ShaderEffectHandle> shader_effects = Material::GetMaterialShaders(m_gaussian_material);
	const RPipelineLayout pipe_layout = BindShaders(a_list, shader_effects);

	FixedArray<PipelineBarrierImageInfo, 2> transitions{};
	PipelineBarrierImageInfo& to_shader_read = transitions[0];
	to_shader_read.src_mask = BARRIER_ACCESS_MASK::COLOR_ATTACHMENT_WRITE;
	to_shader_read.dst_mask = BARRIER_ACCESS_MASK::SHADER_READ;
	to_shader_read.image = a_pfd.bloom.image;
	to_shader_read.old_layout = IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL;
	to_shader_read.new_layout = IMAGE_LAYOUT::SHADER_READ_ONLY;
	to_shader_read.layer_count = 1;
	to_shader_read.level_count = 1;
	to_shader_read.base_array_layer = 0;
	to_shader_read.base_mip_level = 0;
	to_shader_read.src_stage = BARRIER_PIPELINE_STAGE::COLOR_ATTACH_OUTPUT;
	to_shader_read.dst_stage = BARRIER_PIPELINE_STAGE::FRAGMENT_SHADER;
	to_shader_read.aspects = IMAGE_ASPECT::COLOR;

	PipelineBarrierImageInfo& to_render_target = transitions[1];
	to_render_target.src_mask = BARRIER_ACCESS_MASK::SHADER_READ;
	to_render_target.dst_mask = BARRIER_ACCESS_MASK::COLOR_ATTACHMENT_WRITE;
	to_render_target.image = a_pfd.bloom.image;
	to_render_target.old_layout = IMAGE_LAYOUT::SHADER_READ_ONLY;
	to_render_target.new_layout = IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL;
	to_render_target.layer_count = 1;
	to_render_target.level_count = 1;
	to_render_target.base_array_layer = 1;
	to_render_target.base_mip_level = 0;
	to_render_target.src_stage = BARRIER_PIPELINE_STAGE::FRAGMENT_SHADER;
	to_render_target.dst_stage = BARRIER_PIPELINE_STAGE::COLOR_ATTACH_OUTPUT;
	to_render_target.aspects = IMAGE_ASPECT::COLOR;

	PipelineBarrierInfo barrier_info{};
	barrier_info.image_infos = transitions.data();
	barrier_info.image_info_count = transitions.size();

	SetFrontFace(a_list, false);
	SetCullMode(a_list, CULL_MODE::NONE);
	// horizontal bloom slice
	{
		PipelineBarriers(a_list, barrier_info);

		RenderingAttachmentColor color_attach;
		color_attach.load_color = false;
		color_attach.store_color = true;
		color_attach.image_layout = IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL;
		color_attach.image_view = GetImageView(a_pfd.bloom.descriptor_index_1);
		StartRenderingInfo rendering_info;
		rendering_info.color_attachments = Slice(&color_attach, 1);
		rendering_info.depth_attachment = nullptr;
		rendering_info.render_area_extent = a_pfd.bloom.resolution;
		rendering_info.render_area_offset = int2();

		ShaderGaussianBlur push_constant;
		push_constant.horizontal_enable = false;
		push_constant.src_texture = a_pfd.bloom.descriptor_index_0;
		push_constant.src_resolution = a_pfd.bloom.resolution;
		push_constant.blur_strength = m_postfx.bloom_strength;
		push_constant.blur_scale = m_postfx.bloom_scale;

		SetPushConstants(a_list, pipe_layout, 0, sizeof(push_constant), &push_constant);

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
		color_attach.image_layout = IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL;
		color_attach.image_view = a_render_target;
		StartRenderingInfo rendering_info;
		rendering_info.color_attachments = Slice(&color_attach, 1);
		rendering_info.depth_attachment = nullptr;
		rendering_info.render_area_extent = a_draw_area_size;
		rendering_info.render_area_offset = a_draw_area_offset;

		ShaderGaussianBlur push_constant;
		push_constant.horizontal_enable = true;
		push_constant.src_texture = a_pfd.bloom.descriptor_index_1;
		push_constant.src_resolution = a_pfd.bloom.resolution;
		push_constant.blur_strength = m_postfx.bloom_strength;
		push_constant.blur_scale = m_postfx.bloom_scale;
		SetPushConstants(a_list, pipe_layout, 0, sizeof(push_constant), &push_constant);

		StartRenderPass(a_list, rendering_info);
		DrawVertices(a_list, 3, 1, 0, 0);
		EndRenderPass(a_list);
	}
}
