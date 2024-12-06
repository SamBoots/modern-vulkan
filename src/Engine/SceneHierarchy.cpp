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

constexpr float BLOOM_IMAGE_DOWNSCALE_FACTOR = 1.f;

void SceneHierarchy::Init(MemoryArena& a_arena, const uint32_t a_back_buffers, const StringView a_name, const uint32_t a_scene_obj_max)
{
	m_scene_name = a_name;

	{	// ECS systems
		m_ecs_entities.Init(a_arena, a_scene_obj_max);
		m_transform_pool.Init(a_arena, a_scene_obj_max);
		m_render_mesh_pool.Init(a_arena, a_scene_obj_max);
	}

	m_scene_objects.Init(a_arena, a_scene_obj_max);
	m_top_level_objects = ArenaAllocArr(a_arena, SceneObjectHandle, a_scene_obj_max);

	m_top_level_object_count = 0;

	m_options.skip_skybox = false;
	m_options.skip_shadow_mapping = false;
	m_options.skip_object_rendering = false;
	m_options.skip_bloom = false;

	// maybe make this part of some data structure
	// also rework this to be more static and internal to the drawmesh
	m_draw_list.max_size = a_scene_obj_max;
	m_draw_list.size = 0;
	m_draw_list.mesh_draw_call = ArenaAllocArr(a_arena, RenderMesh, m_draw_list.max_size);
	m_draw_list.transform = ArenaAllocArr(a_arena, ShaderTransform, m_draw_list.max_size);

	m_light_container.Init(a_arena, 128); // magic number jank yes shoot me
	m_light_projection_view.Init(a_arena, 128);

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

SceneObjectHandle SceneHierarchy::CreateSceneObjectViaModelNode(const Model& a_model, const Model::Node& a_node, const SceneObjectHandle a_parent)
{
	const SceneObjectHandle scene_handle = m_scene_objects.emplace(SceneObject());
	SceneObject& scene_obj = m_scene_objects.find(scene_handle);
	scene_obj.name = a_node.name;
	scene_obj.light_handle = LightHandle(BB_INVALID_HANDLE_64);
	BB_ASSERT(m_ecs_entities.CreateEntity(scene_obj.entity), "failed to create entity");
	BB_ASSERT(EntityAssignTransform(scene_obj.entity, a_node.translation, a_node.rotation, a_node.scale), "failed to create tranform");
	scene_obj.parent = a_parent;

	if (a_node.mesh)
	{
		const Model::Mesh& mesh = *a_node.mesh;
		for (uint32_t i = 0; i < mesh.primitives.size(); i++)
		{
			BB_ASSERT(scene_obj.child_count <= SCENE_OBJ_CHILD_MAX, "Too many children for a single scene object!");
			SceneObject prim_obj{};
			prim_obj.name = "unimplemented naming";
			RenderMesh mesh_info;
			mesh_info.mesh = mesh.mesh;
			mesh_info.index_start = mesh.primitives[i].start_index;
			mesh_info.index_count = mesh.primitives[i].index_count;
			mesh_info.master_material = mesh.primitives[i].material_data.material;
			mesh_info.material = Material::CreateMaterialInstance(mesh.primitives[i].material_data.material);
			mesh_info.material_data = mesh.primitives[i].material_data.mesh_metallic;
			mesh_info.material_dirty = true;

			scene_obj.light_handle = LightHandle(BB_INVALID_HANDLE_64);
			BB_ASSERT(m_ecs_entities.CreateEntity(prim_obj.entity), "failed to create entity");
			BB_ASSERT(EntityAssignTransform(prim_obj.entity), "failed to create tranform");
			BB_ASSERT(EntityAssignRenderMesh(prim_obj.entity, mesh_info), "failed to create rendermesh");
			prim_obj.parent = scene_handle;
			scene_obj.children[scene_obj.child_count++] = m_scene_objects.emplace(prim_obj);
		}
	}

	for (uint32_t i = 0; i < a_node.child_count; i++)
	{
		BB_ASSERT(scene_obj.child_count < SCENE_OBJ_CHILD_MAX, "Too many childeren for a single gameobject!");
		scene_obj.children[scene_obj.child_count++] = CreateSceneObjectViaModelNode(a_model, a_node.childeren[i], a_parent);
	}

	return scene_handle;
}

SceneObjectHandle SceneHierarchy::CreateSceneObject(const float3 a_position, const char* a_name, const SceneObjectHandle a_parent)
{
	SceneObjectHandle scene_object_handle = m_scene_objects.emplace(SceneObject());
	SceneObject& scene_object = m_scene_objects.find(scene_object_handle);
	if (a_parent.IsValid())
	{
		SceneObject& parent = m_scene_objects.find(a_parent);
		parent.AddChild(scene_object_handle);
		scene_object.parent = a_parent;
	}
	else
	{
		scene_object.parent = SceneObjectHandle(BB_INVALID_HANDLE_64);
		BB_ASSERT(m_top_level_object_count < m_scene_objects.capacity(), "Too many scene objects, increase the max");
		m_top_level_objects[m_top_level_object_count++] = scene_object_handle;
	}

	scene_object.name = a_name;
	scene_object.light_handle = LightHandle(BB_INVALID_HANDLE_64);
	BB_ASSERT(m_ecs_entities.CreateEntity(scene_object.entity), "failed to create entity");
	BB_ASSERT(EntityAssignTransform(scene_object.entity, a_position), "failed to create transform!");
	scene_object.child_count = 0;

	return scene_object_handle;
}

SceneObjectHandle SceneHierarchy::CreateSceneObjectMesh(const float3 a_position, const SceneMeshCreateInfo& a_mesh_info, const char* a_name, const SceneObjectHandle a_parent)
{
	SceneObjectHandle scene_object_handle = m_scene_objects.emplace(SceneObject());
	SceneObject& scene_object = m_scene_objects.find(scene_object_handle);
	if (a_parent.IsValid())
	{
		SceneObject& parent = m_scene_objects.find(a_parent);
		parent.AddChild(scene_object_handle);
		scene_object.parent = a_parent;
	}
	else
	{
		scene_object.parent = SceneObjectHandle(BB_INVALID_HANDLE_64);
		BB_ASSERT(m_top_level_object_count < m_scene_objects.capacity(), "Too many scene objects, increase the max");
		m_top_level_objects[m_top_level_object_count++] = scene_object_handle;
	}

	scene_object.name = a_name;
	RenderMesh mesh_info;
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

	scene_object.light_handle = LightHandle(BB_INVALID_HANDLE_64);
	BB_ASSERT(m_ecs_entities.CreateEntity(scene_object.entity), "failed to create entity");
	BB_ASSERT(EntityAssignTransform(scene_object.entity, a_position), "failed to create transform!");
	BB_ASSERT(EntityAssignRenderMesh(scene_object.entity, mesh_info), "failed to create rendermesh");
	scene_object.child_count = 0;

	return scene_object_handle;
}

SceneObjectHandle SceneHierarchy::CreateSceneObjectViaModel(const Model& a_model, const float3 a_position, const char* a_name, const SceneObjectHandle a_parent)
{
	SceneObjectHandle scene_object_handle = m_scene_objects.emplace(SceneObject());
	SceneObject& scene_object = m_scene_objects.find(scene_object_handle);
	if (a_parent.IsValid())
	{
		SceneObject& parent = m_scene_objects.find(a_parent);
		parent.AddChild(scene_object_handle);
		scene_object.parent = a_parent;
	}
	else
	{
		scene_object.parent = SceneObjectHandle(BB_INVALID_HANDLE_64);
		BB_ASSERT(m_top_level_object_count < m_scene_objects.capacity(), "Too many scene objects, increase the max");
		m_top_level_objects[m_top_level_object_count++] = scene_object_handle;
	}

	scene_object.name = a_name;
	scene_object.light_handle = LightHandle(BB_INVALID_HANDLE_64);
	BB_ASSERT(m_ecs_entities.CreateEntity(scene_object.entity), "failed to create entity");
	BB_ASSERT(EntityAssignTransform(scene_object.entity, a_position), "failed to create transform!");
	scene_object.child_count = 0;

	for (uint32_t i = 0; i < a_model.root_node_count; i++)
	{
		scene_object.AddChild(CreateSceneObjectViaModelNode(a_model, a_model.linear_nodes[a_model.root_node_indices[i]], scene_object_handle));
	}

	return scene_object_handle;
}

SceneObjectHandle SceneHierarchy::CreateSceneObjectAsLight(const LightCreateInfo& a_light_create_info, const char* a_name, const SceneObjectHandle a_parent)
{
	SceneObjectHandle scene_object_handle = m_scene_objects.emplace(SceneObject());
	SceneObject& scene_object = m_scene_objects.find(scene_object_handle);
	if (a_parent.IsValid())
	{
		SceneObject& parent = m_scene_objects.find(a_parent);
		parent.AddChild(scene_object_handle);
		scene_object.parent = a_parent;
	}
	else
	{
		scene_object.parent = SceneObjectHandle(BB_INVALID_HANDLE_64);
		BB_ASSERT(m_top_level_object_count < m_scene_objects.capacity(), "Too many scene objects, increase the max");
		m_top_level_objects[m_top_level_object_count++] = scene_object_handle;
	}

	scene_object.name = a_name;
	
	BB_ASSERT(m_ecs_entities.CreateEntity(scene_object.entity), "failed to create entity");
	BB_ASSERT(EntityAssignTransform(scene_object.entity, a_light_create_info.pos), "failed to create transform!");
	scene_object.child_count = 0;
	scene_object.light_handle = CreateLight(a_light_create_info);

	return scene_object_handle;
}

bool SceneHierarchy::EntityAssignTransform(const ECSEntity a_entity, const float3 a_position, const Quat a_rotation, const float3 a_scale)
{
	if (!m_transform_pool.CreateComponent(a_entity, Transform(a_position, a_rotation, a_scale)))
		return false;
	if (!m_ecs_entities.RegisterSignature(a_entity, m_transform_pool.GetSignatureIndex()))
		return false;
	return true;
}

bool SceneHierarchy::EntityAssignRenderMesh(const ECSEntity a_entity, const RenderMesh& a_draw_info)
{
	if (!m_render_mesh_pool.CreateComponent(a_entity, a_draw_info))
		return false;
	if (!m_ecs_entities.RegisterSignature(a_entity, m_render_mesh_pool.GetSignatureIndex()))
		return false;
	return true;
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

	for (size_t i = 0; i < m_top_level_object_count; i++)
	{
		// identity hack to awkwardly get the first matrix. 
		DrawSceneObject(m_top_level_objects[i], Float4x4Identity(), a_list, pfd);
	}

	UpdateConstantBuffer(pfd, a_list, a_draw_area_size);

	SkyboxPass(pfd, a_list, a_render_target_view, a_draw_area_size, a_draw_area_offset);

	ResourceUploadPass(pfd, a_list);

	ShadowMapPass(pfd, a_list, uint2(DEPTH_IMAGE_SIZE_W_H, DEPTH_IMAGE_SIZE_W_H));

	GeometryPass(pfd, a_list, a_render_target_view, a_draw_area_size, a_draw_area_offset);

	BloomPass(pfd, a_list, a_render_target_view, a_draw_area_size, a_draw_area_offset);
}

void SceneHierarchy::DrawSceneObject(const SceneObjectHandle a_scene_object, const float4x4& a_transform, const RCommandList a_list, const PerFrameData& a_pfd)
{
	SceneObject& scene_object = m_scene_objects.find(a_scene_object);
	const float4x4 local_transform = a_transform * m_transform_pool.GetComponent(scene_object.entity).CreateMatrix();

	// mesh_info should be a pointer in the drawlist. Preferably. 
	if (m_ecs_entities.HasSignature(scene_object.entity, RENDERMESH_ECS_SIGNATURE))
	{
		RenderMesh& rendermesh = m_render_mesh_pool.GetComponent(scene_object.entity);
		AddToDrawList(rendermesh, local_transform);
		if (rendermesh.material_dirty)
		{
			const UploadBuffer upload = m_upload_allocator.AllocateUploadMemory(sizeof(rendermesh.material_data), a_pfd.fence_value);
			upload.SafeMemcpy(0, &rendermesh.material_data, sizeof(rendermesh.material_data));
			Material::WriteMaterial(rendermesh.material, a_list, upload.buffer, upload.base_offset);
			rendermesh.material_dirty = false;
		}
	}

	for (size_t i = 0; i < scene_object.child_count; i++)
	{
		DrawSceneObject(scene_object.children[i], local_transform, a_list, a_pfd);
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

void SceneHierarchy::SkyboxPass(const PerFrameData& a_pfd, const RCommandList a_list, const RImageView a_render_target_view, const uint2 a_draw_area_size, const int2 a_draw_area_offset)
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

void SceneHierarchy::UpdateConstantBuffer(PerFrameData& a_pfd, const RCommandList a_list, const uint2 a_draw_area_size)
{
	m_scene_info.light_count = m_light_container.size();
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

void SceneHierarchy::ResourceUploadPass(PerFrameData& a_pfd, const RCommandList a_list)
{
	GPULinearBuffer& cur_scene_buffer = a_pfd.storage_buffer;
	cur_scene_buffer.Clear();

	{	// write scene info
		a_pfd.scene_buffer.WriteTo(&m_scene_info, sizeof(m_scene_info), 0);
	}

	const size_t matrices_upload_size = sizeof(ShaderTransform) * m_draw_list.size;
	const size_t light_upload_size = sizeof(Light) * m_light_container.size();
	const size_t light_projection_view_size = sizeof(float4x4) * m_light_projection_view.size();
	// optimize this
	const size_t total_size = matrices_upload_size + light_upload_size + light_projection_view_size;

	const UploadBuffer upload_buffer = m_upload_allocator.AllocateUploadMemory(total_size, a_pfd.fence_value);

	size_t bytes_uploaded = 0;

	upload_buffer.SafeMemcpy(bytes_uploaded, m_draw_list.transform, matrices_upload_size);
	const size_t matrix_offset = bytes_uploaded + upload_buffer.base_offset;
	bytes_uploaded += matrices_upload_size;

	upload_buffer.SafeMemcpy(bytes_uploaded, m_light_container.data(), light_upload_size);
	const size_t light_offset = bytes_uploaded + upload_buffer.base_offset;
	bytes_uploaded += light_upload_size;

	upload_buffer.SafeMemcpy(bytes_uploaded, m_light_projection_view.data(), light_projection_view_size);
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

void SceneHierarchy::ShadowMapPass(const PerFrameData& a_pfd, const RCommandList a_list, const uint2 a_shadow_map_resolution)
{
	const uint32_t shadow_map_count = m_light_projection_view.size();
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
				for (uint32_t draw_index = 0; draw_index < m_draw_list.size; draw_index++)
				{
					const RenderMesh& mesh_draw_call = m_draw_list.mesh_draw_call[draw_index];

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

void SceneHierarchy::GeometryPass(const PerFrameData& a_pfd, const RCommandList a_list, const RImageView a_render_target_view, const uint2 a_draw_area_size, const int2 a_draw_area_offset)
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

	for (uint32_t i = 0; i < m_draw_list.size; i++)
	{
		const RenderMesh& mesh_draw_call = m_draw_list.mesh_draw_call[i];

		const ConstSlice<ShaderEffectHandle> shader_effects = Material::GetMaterialShaders(mesh_draw_call.master_material);
		const RPipelineLayout pipe_layout = BindShaders(a_list, shader_effects);
		{
			const uint32_t buffer_indices[] = { 0 };
			const size_t buffer_offsets[]{ Material::GetMaterialDescAllocation().offset};
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

void SceneHierarchy::BloomPass(const PerFrameData& a_pfd, const RCommandList a_list, const RImageView a_render_target, const uint2 a_draw_area_size, const int2 a_draw_area_offset)
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

void SceneHierarchy::AddToDrawList(const RenderMesh& a_render_mesh, const float4x4& a_transform)
{
	BB_ASSERT(m_draw_list.size + 1 < m_draw_list.max_size, "too many drawn elements!");
	m_draw_list.mesh_draw_call[m_draw_list.size] = a_render_mesh;
	m_draw_list.transform[m_draw_list.size].transform = a_transform;
	m_draw_list.transform[m_draw_list.size++].inverse = Float4x4Inverse(a_transform);
}

LightHandle SceneHierarchy::CreateLight(const LightCreateInfo& a_light_info)
{
	Light light;
	light.light_type = static_cast<uint32_t>(a_light_info.light_type);
	light.color = float4(a_light_info.color.x, a_light_info.color.y, a_light_info.color.z, a_light_info.specular_strength);
	light.pos = float4(a_light_info.pos.x, a_light_info.pos.y, a_light_info.pos.z, 0.0f);

	light.radius_constant = a_light_info.radius_constant;
	light.radius_linear = a_light_info.radius_linear;
	light.radius_quadratic = a_light_info.radius_quadratic;

	light.direction = float4(a_light_info.direction.x, a_light_info.direction.y, a_light_info.direction.z, a_light_info.cutoff_radius);

	const LightHandle light_handle = m_light_container.insert(light);

	const float near_plane = 1.f, far_plane = 7.5f;
	const float4x4 vp = CalculateLightProjectionView(a_light_info.pos, near_plane, far_plane);
	const LightHandle light_handle_view = m_light_projection_view.emplace(vp);

	BB_ASSERT(light_handle == light_handle_view, "Something went wrong trying to create a light");

	return light_handle;
}

float4x4 SceneHierarchy::CalculateLightProjectionView(const float3 a_pos, const float a_near, const float a_far) const
{
	const float4x4 projection = Float4x4Perspective(ToRadians(45.f), 1.0f, a_near, a_far);
	const float4x4 view = Float4x4Lookat(a_pos, float3(), float3(0.0f, -1.0f, 0.0f));
	return projection * view;
}

Light& SceneHierarchy::GetLight(const LightHandle a_light) const
{
	return m_light_container.find(a_light);
}

void SceneHierarchy::FreeLight(const LightHandle a_light)
{
	m_light_container.erase(a_light);
}
