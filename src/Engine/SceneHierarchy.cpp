#include "SceneHierarchy.hpp"
#include "Math.inl"
#include "BBjson.hpp"
#include "BBThreadScheduler.hpp"
#include "RendererTypes.hpp"
#include "OS/Program.h"
#include "MaterialSystem.hpp"

#include <vector>

using namespace BB;

// arbitrary, but just for a stack const char array
constexpr size_t UNIQUE_MODELS_PER_SCENE = 128;

constexpr uint32_t INITIAL_DEPTH_ARRAY_COUNT = 8;

constexpr uint32_t DEPTH_IMAGE_SIZE_W_H = 4096;

constexpr uint32_t LIGHT_COUNT = 128;

void SceneHierarchy::Init(MemoryArena& a_arena, const uint32_t a_back_buffers, const StringView a_name, const uint32_t a_scene_obj_max)
{
	m_scene_name = a_name;

	{	// ECS systems
		EntityComponentSystemCreateInfo create_info;
		create_info.entity_count = a_scene_obj_max;
		create_info.light_count = LIGHT_COUNT;
		create_info.render_mesh_count = a_scene_obj_max;
		m_ecs.Init(a_arena, create_info);
	}

	m_scene_objects.Init(a_arena, a_scene_obj_max);

	m_options.skip_skybox = false;
	m_options.skip_shadow_mapping = false;
	m_options.skip_object_rendering = false;
	m_options.skip_bloom = false;

	// maybe make this part of some data structure
	// also rework this to be more static and internal to the drawmesh
	m_draw_list.max_size = a_scene_obj_max;
	m_draw_list.size = 0;
	m_draw_list.mesh_draw_call = ArenaAllocArr(a_arena, RenderComponent, m_draw_list.max_size);
	m_draw_list.transform = ArenaAllocArr(a_arena, ShaderTransform, m_draw_list.max_size);

	m_fence = CreateFence(0, "scene fence");
	m_last_completed_fence_value = 0;
	m_next_fence_value = 1;

	m_upload_allocator.Init(a_arena, mbSize * 4, m_fence, "scene upload buffer");

	m_scene_info.ambient_light = float4(0.05f, 0.05f, 0.05f, 1.f);
	m_scene_info.shadow_map_resolution = float2(DEPTH_IMAGE_SIZE_W_H, DEPTH_IMAGE_SIZE_W_H);

	m_per_frame.Init(a_arena, a_back_buffers);
	m_per_frame.resize(a_back_buffers);
	for (uint32_t i = 0; i < m_per_frame.size(); i++)
	{
		PerFrameData& pfd = m_per_frame[i];
		pfd.previous_draw_area = { 0, 0 };
		pfd.scene_descriptor = AllocateDescriptor(GetSceneDescriptorLayout());

		pfd.scene_buffer.Init(BUFFER_TYPE::UNIFORM, sizeof(m_scene_info), "scene info buffer");
		{
			DescriptorWriteBufferInfo desc_write;
			desc_write.descriptor_layout = GetSceneDescriptorLayout();
			desc_write.allocation = pfd.scene_descriptor;
			desc_write.descriptor_index = 0;

			desc_write.binding = PER_SCENE_SCENE_DATA_BINDING;
			desc_write.buffer_view = pfd.scene_buffer.GetView();
			DescriptorWriteUniformBuffer(desc_write);
		}

		GPUBufferCreateInfo buffer_info;
		buffer_info.name = "scene STORAGE buffer";
		buffer_info.size = mbSize * 4;
		buffer_info.type = BUFFER_TYPE::STORAGE;
		buffer_info.host_writable = false;
		pfd.storage_buffer.Init(buffer_info);

		pfd.fence_value = 0;

		pfd.depth_image = RImage();
		pfd.bloom.image = RImage();

		pfd.shadow_map.render_pass_views.Init(a_arena, INITIAL_DEPTH_ARRAY_COUNT);
		pfd.shadow_map.render_pass_views.resize(INITIAL_DEPTH_ARRAY_COUNT);
		{
			ImageCreateInfo shadow_map_img;
			shadow_map_img.name = "shadow map array";
			shadow_map_img.width = DEPTH_IMAGE_SIZE_W_H;
			shadow_map_img.height = DEPTH_IMAGE_SIZE_W_H;
			shadow_map_img.depth = 1;
			shadow_map_img.array_layers = static_cast<uint16_t>(pfd.shadow_map.render_pass_views.size());
			shadow_map_img.mip_levels = 1;
			shadow_map_img.use_optimal_tiling = true;
			shadow_map_img.type = IMAGE_TYPE::TYPE_2D;
			shadow_map_img.format = IMAGE_FORMAT::D16_UNORM;
			shadow_map_img.usage = IMAGE_USAGE::SHADOW_MAP;
			shadow_map_img.is_cube_map = false;
			pfd.shadow_map.image = CreateImage(shadow_map_img);
		}

		{
			ImageViewCreateInfo shadow_map_img_view;
			shadow_map_img_view.name = "shadow map array view";
			shadow_map_img_view.image = pfd.shadow_map.image;
			shadow_map_img_view.base_array_layer = 0;
			shadow_map_img_view.array_layers = static_cast<uint16_t>(pfd.shadow_map.render_pass_views.size());
			shadow_map_img_view.mip_levels = 1;
			shadow_map_img_view.base_mip_level = 0;
			shadow_map_img_view.format = IMAGE_FORMAT::D16_UNORM;
			shadow_map_img_view.type = IMAGE_VIEW_TYPE::TYPE_2D_ARRAY;
			shadow_map_img_view.aspects = IMAGE_ASPECT::DEPTH;
			pfd.shadow_map.descriptor_index = CreateImageView(shadow_map_img_view);
		}

		{
			ImageViewCreateInfo render_pass_shadow_view{};
			render_pass_shadow_view.name = "shadow map renderpass view";
			render_pass_shadow_view.image = pfd.shadow_map.image;
			render_pass_shadow_view.array_layers = 1;
			render_pass_shadow_view.mip_levels = 1;
			render_pass_shadow_view.base_mip_level = 0;
			render_pass_shadow_view.format = IMAGE_FORMAT::D16_UNORM;
			render_pass_shadow_view.type = IMAGE_VIEW_TYPE::TYPE_2D;
			render_pass_shadow_view.aspects = IMAGE_ASPECT::DEPTH;
			for (uint32_t shadow_index = 0; shadow_index < pfd.shadow_map.render_pass_views.size(); shadow_index++)
			{
				render_pass_shadow_view.base_array_layer = static_cast<uint16_t>(shadow_index);
				pfd.shadow_map.render_pass_views[shadow_index] = CreateImageViewShaderInaccessible(render_pass_shadow_view);
			}
		}
	}

	// shadow map shader

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

	// bloom stuff

	MaterialCreateInfo gaussian_material;
	gaussian_material.pass_type = PASS_TYPE::SCENE;
	gaussian_material.material_type = MATERIAL_TYPE::NONE;
	FixedArray<MaterialShaderCreateInfo, 2> gaussian_shaders;
	gaussian_shaders[0].path = "../../resources/shaders/hlsl/GaussianBlur.hlsl";
	gaussian_shaders[0].entry = "VertexMain";
	gaussian_shaders[0].stage = SHADER_STAGE::VERTEX;
	gaussian_shaders[0].next_stages = static_cast<uint32_t>(SHADER_STAGE::FRAGMENT_PIXEL);
	gaussian_shaders[1].path = "../../resources/shaders/hlsl/GaussianBlur.hlsl";
	gaussian_shaders[1].entry = "FragmentMain";
	gaussian_shaders[1].stage = SHADER_STAGE::FRAGMENT_PIXEL;
	gaussian_shaders[1].next_stages = static_cast<uint32_t>(SHADER_STAGE::NONE);
	gaussian_material.shader_infos = Slice(gaussian_shaders.slice());

	MemoryArenaScope(a_arena)
	{
		m_gaussian_material = Material::CreateMasterMaterial(a_arena, gaussian_material, "shadow map material");
	}

	// skybox stuff
	
	MaterialCreateInfo skybox_material;
	skybox_material.pass_type = PASS_TYPE::SCENE;
	skybox_material.material_type = MATERIAL_TYPE::NONE;
	FixedArray<MaterialShaderCreateInfo, 2> skybox_shaders;
	skybox_shaders[0].path = "../../resources/shaders/hlsl/skybox.hlsl";
	skybox_shaders[0].entry = "VertexMain";
	skybox_shaders[0].stage = SHADER_STAGE::VERTEX;
	skybox_shaders[0].next_stages = static_cast<uint32_t>(SHADER_STAGE::FRAGMENT_PIXEL);
	skybox_shaders[1].path = "../../resources/shaders/hlsl/skybox.hlsl";
	skybox_shaders[1].entry = "FragmentMain";
	skybox_shaders[1].stage = SHADER_STAGE::FRAGMENT_PIXEL;
	skybox_shaders[1].next_stages = static_cast<uint32_t>(SHADER_STAGE::NONE);
	skybox_material.shader_infos = Slice(skybox_shaders.slice());
	MemoryArenaScope(a_arena)
	{
		m_skybox_material = Material::CreateMasterMaterial(a_arena, skybox_material, "skybox material");
	}

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

		constexpr const char* SKY_BOX_NAME[6]
		{
			"../../resources/textures/skybox/0.jpg",
			"../../resources/textures/skybox/1.jpg",
			"../../resources/textures/skybox/2.jpg",
			"../../resources/textures/skybox/3.jpg",
			"../../resources/textures/skybox/4.jpg",
			"../../resources/textures/skybox/5.jpg",
		};

		for (size_t i = 0; i < 6; i++)
		{
			int dummy_x, dummy_y, dummy_bytes_per;
			unsigned char* pixels = Asset::LoadImageCPU(SKY_BOX_NAME[i], dummy_x, dummy_y, dummy_bytes_per);
			BB_ASSERT(static_cast<uint32_t>(dummy_x) == skybox_image_info.width && static_cast<uint32_t>(dummy_y) == skybox_image_info.height && dummy_bytes_per == 4, "skybox dimentions wrong");
			WriteImageInfo write_info{};
			write_info.image = m_skybox;
			write_info.format = IMAGE_FORMAT::RGBA8_SRGB;
			write_info.extent = { skybox_image_info.width, skybox_image_info.height };
			write_info.offset = {};
			write_info.layer_count = 1;
			write_info.base_array_layer = static_cast<uint16_t>(i);
			write_info.set_shader_visible = true;
			write_info.pixels = pixels;
			WriteTexture(write_info);

			Asset::FreeImageCPU(pixels);
		}
	}

	// postfx
	m_postfx.bloom_scale = 1.0f;
	m_postfx.bloom_strength = 1.5f;
}

static bool NameIsWithinCharArray(const char** a_arr, const size_t a_arr_size, const char* a_str)
{
	for (size_t i = 0; i < a_arr_size; i++)
	{
		if (strcmp(a_arr[i], a_str) == 0)
			return true;
	}
	return false;
}

StaticArray<Asset::AsyncAsset> SceneHierarchy::PreloadAssetsFromJson(MemoryArena& a_arena, const JsonParser& a_parsed_file)
{
	const JsonObject& scene_obj = a_parsed_file.GetRootNode()->GetObject().Find("scene")->GetObject();

	const char* unique_models[UNIQUE_MODELS_PER_SCENE]{};
	size_t unique_model_count = 0;

	// first get all the unique models
	const JsonList& scene_objects = scene_obj.Find("scene_objects")->GetList();
	for (size_t i = 0; i < scene_objects.node_count; i++)
	{
		const char* model_name = scene_objects.nodes[i]->GetObject().Find("file_name")->GetString();

		if (!NameIsWithinCharArray(unique_models, unique_model_count, model_name))
		{
			unique_models[unique_model_count++] = model_name;
		}
	}

	StaticArray<Asset::AsyncAsset> async_model_loads{};
	async_model_loads.Init(a_arena, static_cast<uint32_t>(unique_model_count));
	async_model_loads.resize(static_cast<uint32_t>(unique_model_count));

	for (size_t i = 0; i < unique_model_count; i++)
	{
		async_model_loads[i].asset_type = Asset::ASYNC_ASSET_TYPE::MODEL;
		async_model_loads[i].load_type = Asset::ASYNC_LOAD_TYPE::DISK;
		async_model_loads[i].mesh_disk.path = unique_models[i];
	}

	return async_model_loads;
}

ECSEntity SceneHierarchy::CreateEntityViaModelNode(const Model::Node& a_node, const ECSEntity a_parent)
{
	const ECSEntity ecs_obj = m_ecs.CreateEntity(a_node.name, a_parent, a_node.translation, a_node.rotation, a_node.scale);
	m_scene_objects.push_back(ecs_obj);

	if (a_node.mesh)
	{
		const Model::Mesh& mesh = *a_node.mesh;
		for (uint32_t i = 0; i < mesh.primitives.size(); i++)
		{
			ECSEntity prim_obj = m_ecs.CreateEntity("unimplemented naming", ecs_obj);
			RenderComponent mesh_info;
			mesh_info.mesh = mesh.mesh;
			mesh_info.index_start = mesh.primitives[i].start_index;
			mesh_info.index_count = mesh.primitives[i].index_count;
			mesh_info.master_material = mesh.primitives[i].material_data.material;
			mesh_info.material = Material::CreateMaterialInstance(mesh.primitives[i].material_data.material);
			mesh_info.material_data = mesh.primitives[i].material_data.mesh_metallic;
			mesh_info.material_dirty = true;
			bool success = m_ecs.EntityAssignRenderComponent(prim_obj, mesh_info);
			BB_ASSERT(success, "failed to create RenderComponent");

			m_scene_objects.push_back(prim_obj);
		}
	}

	for (uint32_t i = 0; i < a_node.child_count; i++)
	{
		CreateEntityViaModelNode(a_node.childeren[i], ecs_obj);
	}

	return ecs_obj;
}

ECSEntity SceneHierarchy::CreateEntity(const float3 a_position, const NameComponent& a_name, const ECSEntity a_parent)
{
	const ECSEntity ecs_obj = m_ecs.CreateEntity(a_name, a_parent, a_position);

	m_scene_objects.push_back(ecs_obj);

	return ecs_obj;
}

ECSEntity SceneHierarchy::CreateEntityMesh(const float3 a_position, const SceneMeshCreateInfo& a_mesh_info, const char* a_name, const ECSEntity a_parent)
{
	RenderComponent mesh_info;
	mesh_info.mesh = a_mesh_info.mesh;
	mesh_info.index_start = a_mesh_info.index_start;
	mesh_info.index_count = a_mesh_info.index_count;
	mesh_info.master_material = a_mesh_info.master_material;
	mesh_info.material_data = a_mesh_info.material_data;
	if (mesh_info.master_material.IsValid())
	{
		mesh_info.material = Material::CreateMaterialInstance(a_mesh_info.master_material);
		mesh_info.material_dirty = true;
	}
	else
	{
		mesh_info.material = MaterialHandle(BB_INVALID_HANDLE_64);
		mesh_info.material_dirty = false;
	}

	const ECSEntity ecs_obj = m_ecs.CreateEntity(a_name, a_parent, a_position);
	bool success = m_ecs.EntityAssignRenderComponent(ecs_obj, mesh_info);
	BB_ASSERT(success, "failed to create RenderComponent");

	m_scene_objects.push_back(ecs_obj);

	return ecs_obj;
}

ECSEntity SceneHierarchy::CreateEntityViaModel(const Model& a_model, const float3 a_position, const char* a_name, const ECSEntity a_parent)
{
	ECSEntity ecs_obj;

	const ECSEntity ecs_obj = m_ecs.CreateEntity(a_name, a_parent, a_position);

	m_scene_objects.push_back(ecs_obj);

	for (uint32_t i = 0; i < a_model.root_node_count; i++)
	{
		CreateEntityViaModelNode(a_model.linear_nodes[a_model.root_node_indices[i]], ecs_obj);
	}

	return ecs_obj;
}

ECSEntity SceneHierarchy::CreateEntityAsLight(const LightCreateInfo& a_light_create_info, const char* a_name, const ECSEntity a_parent)
{
	ECSEntity ecs_obj;
	
	bool success = m_ecs_entities.CreateEntity(ecs_obj, a_parent);
	BB_ASSERT(success, "failed to create entity");
	success = EntityAssignName(ecs_obj, a_name);
	BB_ASSERT(success, "failed to create name");
	success = EntityAssignTransform(ecs_obj, a_light_create_info.pos);
	BB_ASSERT(success, "failed to create transform!");
	success = CreateLight(ecs_obj, a_light_create_info);
	BB_ASSERT(success, "failed to create light!");

	m_scene_objects.push_back(ecs_obj);

	return ecs_obj;
}

void SceneHierarchy::SetView(const float4x4& a_view, const float3& a_view_position)
{
	m_scene_info.view = a_view;
	m_scene_info.view_pos = float4(a_view_position.x, a_view_position.y, a_view_position.z, 0.0);
}

void SceneHierarchy::SetProjection(const float4x4& a_projection)
{
	m_scene_info.proj = a_projection;
}

void SceneHierarchy::DrawSceneHierarchy(const RCommandList a_list, const RImageView a_render_target_view, const uint32_t a_back_buffer_index, const uint2 a_draw_area_size, const int2 a_draw_area_offset)
{
	PerFrameData& pfd = m_per_frame[a_back_buffer_index];
	WaitFence(m_fence, pfd.fence_value);
	pfd.fence_value = m_next_fence_value;

	m_draw_list.size = 0;

	for (size_t i = 0; i < m_scene_objects.size(); i++)
	{
		// identity hack to awkwardly get the first matrix. 
		DrawSceneObject(m_scene_objects[i], a_list, pfd);
	}

	UpdateConstantBuffer(pfd, a_list, a_draw_area_size);

	SkyboxPass(pfd, a_list, a_render_target_view, a_draw_area_size, a_draw_area_offset);

	ResourceUploadPass(pfd, a_list);

	ShadowMapPass(pfd, a_list, uint2(DEPTH_IMAGE_SIZE_W_H, DEPTH_IMAGE_SIZE_W_H));

	GeometryPass(pfd, a_list, a_render_target_view, a_draw_area_size, a_draw_area_offset);

	BloomPass(pfd, a_list, a_render_target_view, a_draw_area_size, a_draw_area_offset);
}

void SceneHierarchy::DrawSceneObject(const ECSEntity& a_ecs_obj, const RCommandList a_list, const PerFrameData& a_pfd)
{
	float4x4 local_transform = m_transform_pool.GetComponent(a_ecs_obj).CreateMatrix();
	ECSEntity parent;
	bool success = m_ecs_entities.GetParent(a_ecs_obj, parent);
	BB_ASSERT(success, "failed to get parent!");
	// optimize this shit
	while (parent.IsValid())
	{  
		const ECSEntity p = parent;
		local_transform = local_transform * m_transform_pool.GetComponent(p).CreateMatrix();
		success = m_ecs_entities.GetParent(p, parent);
		BB_ASSERT(success, "failed to get parent!");
	}

	// mesh_info should be a pointer in the drawlist. Preferably. 
	if (m_ecs_entities.HasSignature(a_ecs_obj, RENDER_ECS_SIGNATURE))
	{
		RenderComponent& RenderComponent = m_render_mesh_pool.GetComponent(a_ecs_obj);
		AddToDrawList(RenderComponent, local_transform);
		if (RenderComponent.material_dirty)
		{
			const UploadBuffer upload = m_upload_allocator.AllocateUploadMemory(sizeof(RenderComponent.material_data), a_pfd.fence_value);
			upload.SafeMemcpy(0, &RenderComponent.material_data, sizeof(RenderComponent.material_data));
			Material::WriteMaterial(RenderComponent.material, a_list, upload.buffer, upload.base_offset);
			RenderComponent.material_dirty = false;
		}
	}
}

RDescriptorLayout SceneHierarchy::GetSceneDescriptorLayout()
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

void SceneHierarchy::AddToDrawList(const RenderComponent& a_render_mesh, const float4x4& a_transform)
{
	BB_ASSERT(m_draw_list.size + 1 < m_draw_list.max_size, "too many drawn elements!");
	m_draw_list.mesh_draw_call[m_draw_list.size] = a_render_mesh;
	m_draw_list.transform[m_draw_list.size].transform = a_transform;
	m_draw_list.transform[m_draw_list.size++].inverse = Float4x4Inverse(a_transform);
}

bool SceneHierarchy::CreateLight(const ECSEntity a_entity, const LightCreateInfo& a_light_info)
{
	Light light;
	light.light_type = static_cast<uint32_t>(a_light_info.light_type);
	light.color = float4(a_light_info.color.x, a_light_info.color.y, a_light_info.color.z, a_light_info.specular_strength);
	light.pos = float4(a_light_info.pos.x, a_light_info.pos.y, a_light_info.pos.z, 0.0f);

	light.radius_constant = a_light_info.radius_constant;
	light.radius_linear = a_light_info.radius_linear;
	light.radius_quadratic = a_light_info.radius_quadratic;

	light.direction = float4(a_light_info.direction.x, a_light_info.direction.y, a_light_info.direction.z, a_light_info.cutoff_radius);

	const float near_plane = 1.f, far_plane = 7.5f;
	const float4x4 vp = CalculateLightProjectionView(a_light_info.pos, near_plane, far_plane);

	LightComponent light_component;
	light_component.light = light;
	light_component.projection_view = vp;
	return EntityAssignLight(a_entity, light_component);
}

float4x4 SceneHierarchy::CalculateLightProjectionView(const float3 a_pos, const float a_near, const float a_far) const
{
	const float4x4 projection = Float4x4Perspective(ToRadians(45.f), 1.0f, a_near, a_far);
	const float4x4 view = Float4x4Lookat(a_pos, float3(), float3(0.0f, -1.0f, 0.0f));
	return projection * view;
}

Light& SceneHierarchy::GetLight(const ECSEntity a_entity) const
{
	return m_light_pool.GetComponent(a_entity).light;
}

bool SceneHierarchy::FreeLight(const ECSEntity a_entity)
{
	return m_light_pool.FreeComponent(a_entity);
}
