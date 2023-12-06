#include "Renderer.hpp"
#include "Backend/BackendRendererTypes.hpp"
#include "Backend/BackendRenderer.hpp"

#include "Storage/Slotmap.h"

#include "Math.inl"

using namespace BB;

struct Mesh
{
	GPUBufferView vertex_buffer;
	GPUBufferView index_buffer;
};

struct ShaderEffect
{
	const char* name;				//8
	ShaderObject shader_object;		//16
	RPipelineLayout pipeline_layout;//24
	SHADER_STAGE shader_stage;		//28
	SHADER_STAGE_FLAGS shader_stages_next; //32
};

struct Material
{
	struct Shader
	{
		RPipelineLayout pipeline_layout;

		ShaderObject shader_objects[UNIQUE_SHADER_STAGE_COUNT];
		SHADER_STAGE shader_stages[UNIQUE_SHADER_STAGE_COUNT];
		uint32_t shader_effect_count;
	} shader;


	RTexture base_color;
	RTexture normal_texture;
};

struct MeshDrawCall
{
	MeshHandle mesh;
	MaterialHandle material;
	uint32_t index_start;
	uint32_t index_count;
};

struct DrawList
{
	MeshDrawCall* mesh_draw_call;
	ShaderTransform* transform;
};

struct RenderInterface_inst
{
	SceneInfo scene_info;

	StaticSlotmap<Mesh, MeshHandle> mesh_map{};
	StaticSlotmap<ShaderEffect, ShaderEffectHandle> shader_effect_map{};
	StaticSlotmap<Material, MaterialHandle> material_map{};

	RDescriptorLayout frame_descriptor_layout; //set 2

	struct Frame
	{
		GPUBufferView per_frame_buffer;
		GPUBufferView scene_buffer;
		GPUBufferView transform_buffer;
		DescriptorAllocation desc_alloc;
		uint64_t fence_value;
		RTexture back_buffer_image;
	} *frames;

	RImage depth_image;
	RImageView depth_image_view;

	uint32_t draw_list_count;
	uint32_t draw_list_max;
	DrawList draw_list_data;
};

static RenderInterface_inst* s_render_inst;

void BB::Render(const RCommandList a_cmd_list, const uint2 a_render_area)
{
	//render
	StartRenderingInfo start_rendering_info;
	start_rendering_info.viewport_width = a_render_area.x;
	start_rendering_info.viewport_height = a_render_area.y;
	start_rendering_info.depth_view = s_render_inst->depth_image_view;
	start_rendering_info.layout = IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL;
	start_rendering_info.load_color = false;
	start_rendering_info.store_color = true;
	start_rendering_info.clear_color_rgba = float4{ 0.f, 0.5f, 0.f, 1.f };
	StartRenderPass(a_cmd_list, start_rendering_info);

	{
		//set the first data to get the first 3 descriptor sets.
		const MeshDrawCall& mesh_draw_call = s_render_inst->draw_list_data.mesh_draw_call[0];
		const Material& material = s_render_inst->material_map.find(mesh_draw_call.material);


		//SET THE SECOND SET HERE (PER FRAME)
	}

	BindIndexBuffer(a_cmd_list, s_render_inst->index_buffer.buffer, 0);

	for (uint32_t i = 0; i < s_render_inst->draw_list_count; i++)
	{
		const MeshDrawCall& mesh_draw_call = s_render_inst->draw_list_data.mesh_draw_call[i];
		const Material& material = s_render_inst->material_map.find(mesh_draw_call.material);
		const Mesh& mesh = s_render_inst->mesh_map.find(mesh_draw_call.mesh);

		BindShaderEffects(a_cmd_list,
			material.shader.shader_effect_count,
			material.shader.shader_stages,
			material.shader.shader_objects);

		ShaderIndices shader_indices;
		shader_indices.transform_index = i;
		shader_indices.vertex_buffer_offset = static_cast<uint32_t>(mesh.vertex_buffer.offset);
		shader_indices.albedo_texture = material.base_color.handle;

		SetPushConstants(a_cmd_list, material.shader.pipeline_layout, 0, sizeof(ShaderIndices), &shader_indices);

		DrawIndexed(a_cmd_list,
			static_cast<uint32_t>(mesh.index_buffer.size / sizeof(uint32_t)) + mesh_draw_call.index_count,
			1,
			static_cast<uint32_t>(mesh.index_buffer.offset / sizeof(uint32_t)) + mesh_draw_call.index_start,
			0,
			0);
	}

	EndRenderPass(a_cmd_list);
}

void BB::SetView(const float4x4& a_view)
{
	s_render_inst->scene_info.view = a_view;
}

void BB::SetProjection(const float4x4& a_proj)
{
	s_render_inst->scene_info.proj = a_proj;
}

const MeshHandle BB::CreateMesh(const RCommandList a_list, const CreateMeshInfo& a_create_info, UploadBufferView& a_upload_view)
{
	Mesh mesh;
	mesh.vertex_buffer = AllocateFromVertexBuffer(a_create_info.vertices.sizeInBytes());
	mesh.index_buffer = AllocateFromIndexBuffer(a_create_info.indices.sizeInBytes());

	uint32_t vertex_offset;
	a_upload_view.AllocateAndMemoryCopy(
		a_create_info.vertices.data(),
		static_cast<uint32_t>(a_create_info.vertices.sizeInBytes()),
		vertex_offset);

	uint32_t index_offset;
	a_upload_view.AllocateAndMemoryCopy(
		a_create_info.indices.data(),
		static_cast<uint32_t>(a_create_info.indices.sizeInBytes()),
		index_offset);

	RenderCopyBufferRegion copy_regions[2];
	RenderCopyBuffer copy_buffer_infos[2];

	copy_buffer_infos[0].dst = mesh.vertex_buffer.buffer;
	copy_buffer_infos[0].src = a_upload_view.GetBufferHandle();
	copy_regions[0].size = mesh.vertex_buffer.size;
	copy_regions[0].dst_offset = mesh.vertex_buffer.offset;
	copy_regions[0].src_offset = vertex_offset;
	copy_buffer_infos[0].regions = Slice(&copy_regions[0], 1);

	copy_buffer_infos[1].dst = mesh.index_buffer.buffer;
	copy_buffer_infos[1].src = a_upload_view.GetBufferHandle();
	copy_regions[1].size = mesh.index_buffer.size;
	copy_regions[1].dst_offset = mesh.index_buffer.offset;
	copy_regions[1].src_offset = index_offset;
	copy_buffer_infos[1].regions = Slice(&copy_regions[1], 1);

	CopyBuffers(a_list, copy_buffer_infos, 2);

	return MeshHandle(s_render_inst->mesh_map.insert(mesh).handle);
}

void BB::FreeMesh(const MeshHandle a_mesh)
{
	s_render_inst->mesh_map.erase(a_mesh);
}

bool BB::CreateShaderEffect(Allocator a_temp_allocator, const Slice<CreateShaderEffectInfo> a_create_infos, ShaderEffectHandle* a_handles)
{
	//our default layouts
	RDescriptorLayout desc_layouts[] = {
			s_render_inst->static_sampler_descriptor_set,
			s_render_inst->global_descriptor_set,
			frame_descriptor_layout };

	//all of them use this push constant for the shader indices.
	PushConstantRange push_constant;
	push_constant.stages = SHADER_STAGE::ALL;
	push_constant.offset = 0;

	ShaderEffect* shader_effects = BBnewArr(a_temp_allocator, a_create_infos.size(), ShaderEffect);
	ShaderCode* shader_codes = BBnewArr(a_temp_allocator, a_create_infos.size(), ShaderCode);
	ShaderObjectCreateInfo* shader_object_infos = BBnewArr(a_temp_allocator, a_create_infos.size(), ShaderObjectCreateInfo);

	for (size_t i = 0; i < a_create_infos.size(); i++)
	{
		push_constant.size = a_create_infos[i].push_constant_space;
		shader_effects[i].pipeline_layout = Vulkan::CreatePipelineLayout(desc_layouts, _countof(desc_layouts), &push_constant, 1);

		shader_codes[i] = CompileShader(s_render_inst->shader_compiler,
			a_create_infos[i].shader_path,
			a_create_infos[i].shader_entry,
			a_create_infos[i].stage);
		Buffer shader_buffer = GetShaderCodeBuffer(shader_codes[i]);

		shader_object_infos[i].stage = a_create_infos[i].stage;
		shader_object_infos[i].next_stages = a_create_infos[i].next_stages;
		shader_object_infos[i].shader_code_size = shader_buffer.size;
		shader_object_infos[i].shader_code = shader_buffer.data;
		shader_object_infos[i].shader_entry = a_create_infos[i].shader_entry;

		shader_object_infos[i].descriptor_layout_count = _countof(desc_layouts);
		shader_object_infos[i].descriptor_layouts = desc_layouts;
		shader_object_infos[i].push_constant_range_count = 1;
		shader_object_infos[i].push_constant_ranges = &push_constant;
	}

	ShaderObject* shader_objects = BBnewArr(a_temp_allocator, a_create_infos.size(), ShaderObject);
	Vulkan::CreateShaderObject(a_temp_allocator, Slice(shader_object_infos, a_create_infos.size()), shader_objects);

	for (size_t i = 0; i < a_create_infos.size(); i++)
	{
		shader_effects[i].name = shader_effects[i].name;
		shader_effects[i].shader_object = shader_objects[i];
		shader_effects[i].shader_stage = a_create_infos[i].stage;
		shader_effects[i].shader_stages_next = a_create_infos[i].next_stages;
		a_handles[i] = s_render_inst->shader_effect_map.insert(shader_effects[i]);
		ReleaseShaderCode(shader_codes[i]);
	}
	return true;
}

void BB::FreeShaderEffect(const ShaderEffectHandle a_shader_effect)
{
	ShaderEffect shader_effect = s_render_inst->shader_effect_map.find(a_shader_effect);
	Vulkan::DestroyShaderObject(shader_effect.shader_object);
	Vulkan::FreePipelineLayout(shader_effect.pipeline_layout);
	s_render_inst->shader_effect_map.erase(a_shader_effect);
}

const MaterialHandle BB::CreateMaterial(const CreateMaterialInfo& a_create_info)
{
	BB_ASSERT(UNIQUE_SHADER_STAGE_COUNT >= a_create_info.shader_effects.size(), "too many shader stages!");
	BB_ASSERT(UNIQUE_SHADER_STAGE_COUNT != 0, "no shader effects in material!");

	Material mat;
	//get the first pipeline layout, compare it with all of the ones in the other shaders.
	RPipelineLayout chosen_layout = s_render_inst->shader_effect_map.find(a_create_info.shader_effects[0]).pipeline_layout;

	SHADER_STAGE_FLAGS valid_next_stages = static_cast<uint32_t>(SHADER_STAGE::ALL);
	for (size_t i = 0; i < a_create_info.shader_effects.size(); i++)
	{
		//maybe check if we have duplicate shader stages;
		const ShaderEffect& effect = s_render_inst->shader_effect_map.find(a_create_info.shader_effects[i]);
		BB_ASSERT(chosen_layout == effect.pipeline_layout, "pipeline layouts are not the same for the shader effects");

		if (i < a_create_info.shader_effects.size())
		{
			BB_ASSERT((valid_next_stages & static_cast<SHADER_STAGE_FLAGS>(effect.shader_stage)) == static_cast<SHADER_STAGE_FLAGS>(effect.shader_stage),
				"shader stage is not valid for the next shader stage of the previous shader object");
			valid_next_stages = effect.shader_stages_next;
		}

		mat.shader.shader_objects[i] = effect.shader_object;
		mat.shader.shader_stages[i] = effect.shader_stage;
	}
	mat.shader.shader_effect_count = static_cast<uint32_t>(a_create_info.shader_effects.size());
	mat.shader.pipeline_layout = chosen_layout;
	mat.base_color = a_create_info.base_color;
	mat.normal_texture = a_create_info.normal_texture;

	return MaterialHandle(s_render_inst->material_map.insert(mat).handle);
}

void BB::FreeMaterial(const MaterialHandle a_material)
{
	//maybe go and check the refcount of the textures to possibly free them.
	s_render_inst->material_map.erase(a_material);
}

//maybe not handle a_upload_view_offset
const RTexture BB::UploadTexture(const RCommandList a_list, const UploadImageInfo& a_upload_info, UploadBufferView& a_upload_view)
{
	return BackendUploadTexture(a_list, a_upload_info, a_upload_view);
}

void BB::FreeTexture(const RTexture a_texture)
{
	BackendFreeTexture(a_texture);
}

void BB::DrawMesh(const MeshHandle a_mesh, const float4x4& a_transform, const uint32_t a_index_start, const uint32_t a_index_count, const MaterialHandle a_material)
{
	s_render_inst->draw_list_data.mesh_draw_call[s_render_inst->draw_list_count].mesh = a_mesh;
	s_render_inst->draw_list_data.mesh_draw_call[s_render_inst->draw_list_count].material = a_material;
	s_render_inst->draw_list_data.mesh_draw_call[s_render_inst->draw_list_count].index_start = a_index_start;
	s_render_inst->draw_list_data.mesh_draw_call[s_render_inst->draw_list_count].index_count = a_index_count;
	s_render_inst->draw_list_data.transform[s_render_inst->draw_list_count].transform = a_transform;
	s_render_inst->draw_list_data.transform[s_render_inst->draw_list_count++].inverse = Float4x4Inverse(a_transform);
}
