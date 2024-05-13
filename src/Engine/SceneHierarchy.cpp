#include "SceneHierarchy.hpp"
#include "Math.inl"
#include "BBjson.hpp"
#include "BBThreadScheduler.hpp"

#include "imgui.h"

#include <vector>

using namespace BB;

// arbritrary, but just for a stack const char array
constexpr size_t UNIQUE_MODELS_PER_SCENE = 128;

void SceneHierarchy::Init(MemoryArena& a_arena, const StringView a_name, const uint32_t a_scene_obj_max)
{
	m_transform_pool.Init(a_arena, a_scene_obj_max);
	m_scene_objects.Init(a_arena, a_scene_obj_max);
	m_top_level_objects = ArenaAllocArr(a_arena, SceneObjectHandle, a_scene_obj_max);

	m_top_level_object_count = 0;

	CreateTextureInfo texture_info;
	texture_info.name = "skybox";
	texture_info.usage = IMAGE_USAGE::TEXTURE;
	texture_info.format = IMAGE_FORMAT::RGBA8_SRGB;
	texture_info.width = 2048;
	texture_info.height = 2048;
	texture_info.array_layers = 6;

	m_skybox = CreateTextureCubeMap(texture_info);

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
		BB_ASSERT(static_cast<uint32_t>(dummy_x) == texture_info.width && static_cast<uint32_t>(dummy_y) == texture_info.height && dummy_bytes_per == 4, "skybox dimentions wrong");
		WriteTextureInfo write_info{};
		write_info.extent = { texture_info.width, texture_info.height };
		write_info.offset = {};
		write_info.layer_count = 1;
		write_info.base_array_layer = static_cast<uint16_t>(i);
		write_info.set_shader_visible = true;
		write_info.pixels = pixels;
		WriteTexture(m_skybox, write_info);

		Asset::FreeImageCPU(pixels);
	}

	// maybe make this part of some data structure
	// also rework this to be more static and internal to the drawmesh
	m_draw_list.max_size = a_scene_obj_max;
	m_draw_list.size = 0;
	m_draw_list.mesh_draw_call = ArenaAllocArr(a_arena, MeshDrawCall, m_draw_list.max_size);
	m_draw_list.transform = ArenaAllocArr(a_arena, ShaderTransform, m_draw_list.max_size);

	m_previous_draw_area = { 0, 0 };

	m_light_container.Init(a_arena, 128); // magic number jank yes shoot me

	//per-frame descriptor set 1 for renderpass
	DescriptorBindingInfo descriptor_bindings[3];
	descriptor_bindings[0].binding = PER_SCENE_SCENE_DATA_BINDING;
	descriptor_bindings[0].count = 1;
	descriptor_bindings[0].shader_stage = SHADER_STAGE::ALL;
	descriptor_bindings[0].type = DESCRIPTOR_TYPE::READONLY_CONSTANT;

	descriptor_bindings[1].binding = PER_SCENE_TRANSFORM_DATA_BINDING;
	descriptor_bindings[1].count = 1;
	descriptor_bindings[1].shader_stage = SHADER_STAGE::VERTEX;
	descriptor_bindings[1].type = DESCRIPTOR_TYPE::READONLY_CONSTANT;

	descriptor_bindings[2].binding = PER_SCENE_LIGHT_DATA_BINDING;
	descriptor_bindings[2].count = 1;
	descriptor_bindings[2].shader_stage = SHADER_STAGE::FRAGMENT_PIXEL;
	descriptor_bindings[2].type = DESCRIPTOR_TYPE::READONLY_CONSTANT;
	m_scene_descriptor_layout = CreateDescriptorLayout(a_arena, Slice(descriptor_bindings, _countof(descriptor_bindings)));

	const size_t backbuffer_count = GetRenderIO().frame_count;
	m_per_frame.fence = CreateFence(0, "scene fence");

	m_per_frame.scene_info.ambient_light = float3(1.f, 1.f, 1.f);
	m_per_frame.scene_info.ambient_strength = 1;
	m_per_frame.scene_info.skybox_texture = m_skybox.handle;

	m_per_frame.uniform_buffer.Init(a_arena, backbuffer_count);
	for (size_t i = 0; i < m_per_frame.uniform_buffer.size(); i++)
	{
		GPUBufferCreateInfo buffer_info;
		buffer_info.name = "scene uniform buffer";
		buffer_info.size = mbSize * 4;
		buffer_info.type = BUFFER_TYPE::UNIFORM;
		buffer_info.host_writable = false;
		m_per_frame.uniform_buffer[i].Init(buffer_info);
	}
	m_per_frame.frame_allocator.Init(a_arena, mbSize * 4, m_per_frame.fence, "scene upload buffer");

	m_scene_name = StringView(a_name.c_str(), a_name.size());
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

void BB::SceneHierarchy::InitViaJson(MemoryArena& a_arena, const char* a_json_path, const uint32_t a_scene_obj_max)
{
	JsonParser json_file(a_json_path);
	InitViaJson(a_arena, json_file, a_scene_obj_max);
}

void BB::SceneHierarchy::InitViaJson(MemoryArena& a_arena, const JsonParser& a_parsed_file, const uint32_t a_scene_obj_max)
{
	const JsonObject& scene_obj = a_parsed_file.GetRootNode()->GetObject().Find("scene")->GetObject();

	{
		const JsonNode& name_node = *scene_obj.Find("name");
		const StringView scene_name = Asset::FindOrCreateString(name_node.GetString());
		Init(a_arena, scene_name, a_scene_obj_max);
	}

	const JsonList& scene_objects = scene_obj.Find("scene_objects")->GetList();

	for (size_t i = 0; i < scene_objects.node_count; i++)
	{
		const JsonObject& sce_obj = scene_objects.nodes[i]->GetObject();
		const char* model_name = scene_objects.nodes[i]->GetObject().Find("file_name")->GetString();
		const char* obj_name = scene_objects.nodes[i]->GetObject().Find("file_name")->GetString();
		const Model* model = Asset::FindModelByName(model_name);
		BB_ASSERT(model != nullptr, "model failed to be found");
		const JsonList& position_list = sce_obj.Find("position")->GetList();
		BB_ASSERT(position_list.node_count == 3, "scene_object position in scene json is not 3 elements");
		float3 position;
		position.x = position_list.nodes[0]->GetNumber();
		position.y = position_list.nodes[1]->GetNumber();
		position.z = position_list.nodes[2]->GetNumber();

		CreateSceneObjectViaModel(*model, position, obj_name);
	}

	const JsonList& lights = scene_obj.Find("lights")->GetList();
	for (size_t i = 0; i < lights.node_count; i++)
	{
		const JsonObject& light_obj = lights.nodes[i]->GetObject();
		CreateLightInfo light_info;

		const char* light_type = light_obj.Find("light_type")->GetString();
		if (strcmp(light_type, "spotlight") == 0)
			light_info.light_type = LIGHT_TYPE::SPOT_LIGHT;
		else if (strcmp(light_type, "pointlight") == 0)
			light_info.light_type = LIGHT_TYPE::POINT_LIGHT;
		else
			BB_ASSERT(false, "invalid light type in json");

		const JsonList& position = light_obj.Find("position")->GetList();
		BB_ASSERT(position.node_count == 3, "light position in scene json is not 3 elements");
		light_info.pos.x = position.nodes[0]->GetNumber();
		light_info.pos.y = position.nodes[1]->GetNumber();
		light_info.pos.z = position.nodes[2]->GetNumber();

		const JsonList& color = light_obj.Find("color")->GetList();
		BB_ASSERT(color.node_count == 3, "light color in scene json is not 3 elements");
		light_info.color.x = color.nodes[0]->GetNumber();
		light_info.color.y = color.nodes[1]->GetNumber();
		light_info.color.z = color.nodes[2]->GetNumber();

		light_info.specular_strength = light_obj.Find("specular_strength")->GetNumber();
		light_info.radius_constant = light_obj.Find("constant")->GetNumber();
		light_info.radius_linear = light_obj.Find("linear")->GetNumber();
		light_info.radius_quadratic = light_obj.Find("quadratic")->GetNumber();

		if (light_info.light_type == LIGHT_TYPE::SPOT_LIGHT)
		{
			const JsonList& spot_dir = light_obj.Find("spotlight_dir")->GetList();
			BB_ASSERT(color.node_count == 3, "light spotlight_dir in scene json is not 3 elements");
			light_info.spotlight_direction.x = spot_dir.nodes[0]->GetNumber();
			light_info.spotlight_direction.y = spot_dir.nodes[1]->GetNumber();
			light_info.spotlight_direction.z = spot_dir.nodes[2]->GetNumber();

			light_info.cutoff_radius = light_obj.Find("cutoff_radius")->GetNumber();
		}

		const StringView light_name = Asset::FindOrCreateString(light_obj.Find("name")->GetString());
		CreateSceneObjectAsLight(light_info, light_name.c_str());
	}
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
	scene_obj.mesh_handle = MeshHandle(BB_INVALID_HANDLE_64);
	scene_obj.start_index = 0;
	scene_obj.index_count = 0;
	scene_obj.material = MaterialHandle(BB_INVALID_HANDLE_64);
	scene_obj.light_handle = LightHandle(BB_INVALID_HANDLE_64);
	scene_obj.transform = m_transform_pool.CreateTransform(a_node.translation, a_node.rotation, a_node.scale);
	scene_obj.parent = a_parent;

	if (a_node.mesh)
	{
		const Model::Mesh& mesh = *a_node.mesh;
		for (uint32_t i = 0; i < mesh.primitives.size(); i++)
		{
			BB_ASSERT(scene_obj.child_count <= SCENE_OBJ_CHILD_MAX, "Too many childeren for a single scene object!");
			SceneObject prim_obj{};
			prim_obj.name = mesh.primitives[i].name;
			prim_obj.mesh_handle = mesh.mesh_handle;
			prim_obj.start_index = mesh.primitives[i].start_index;
			prim_obj.index_count = mesh.primitives[i].index_count;
			prim_obj.albedo_texture = mesh.primitives[i].material_data.base_texture;
			prim_obj.normal_texture = mesh.primitives[i].material_data.normal_texture;
			prim_obj.material = GetStandardMaterial();

			scene_obj.light_handle = LightHandle(BB_INVALID_HANDLE_64);
			prim_obj.transform = m_transform_pool.CreateTransform(float3(0, 0, 0));

			prim_obj.parent = scene_handle;
			scene_obj.childeren[scene_obj.child_count++] = m_scene_objects.emplace(prim_obj);
		}
	}

	for (uint32_t i = 0; i < a_node.child_count; i++)
	{
		BB_ASSERT(scene_obj.child_count < SCENE_OBJ_CHILD_MAX, "Too many childeren for a single gameobject!");
		scene_obj.childeren[scene_obj.child_count++] = CreateSceneObjectViaModelNode(a_model, a_node.childeren[i], a_parent);
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
	scene_object.mesh_handle = MeshHandle(BB_INVALID_HANDLE_64);
	scene_object.start_index = 0;
	scene_object.index_count = 0;
	scene_object.material = MaterialHandle(BB_INVALID_HANDLE_64);
	scene_object.light_handle = LightHandle(BB_INVALID_HANDLE_64);
	scene_object.transform = m_transform_pool.CreateTransform(a_position);
	scene_object.child_count = 0;

	return scene_object_handle;
}

SceneObjectHandle SceneHierarchy::CreateSceneObjectMesh(const float3 a_position, const MeshHandle a_mesh, const uint32_t a_start_index, const uint32_t a_index_count, const MaterialHandle a_material, const char* a_name, const SceneObjectHandle a_parent)
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
	scene_object.mesh_handle = a_mesh;
	scene_object.start_index = a_start_index;
	scene_object.index_count = a_index_count;
	scene_object.material = a_material;
	scene_object.light_handle = LightHandle(BB_INVALID_HANDLE_64);
	scene_object.transform = m_transform_pool.CreateTransform(a_position);
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
	scene_object.mesh_handle = MeshHandle(BB_INVALID_HANDLE_64);
	scene_object.start_index = 0;
	scene_object.index_count = 0;
	scene_object.material = MaterialHandle(BB_INVALID_HANDLE_64);
	scene_object.light_handle = LightHandle(BB_INVALID_HANDLE_64);
	scene_object.transform = m_transform_pool.CreateTransform(a_position);
	scene_object.child_count = 0;

	for (uint32_t i = 0; i < a_model.root_node_count; i++)
	{
		scene_object.AddChild(CreateSceneObjectViaModelNode(a_model, a_model.linear_nodes[a_model.root_node_indices[i]], scene_object_handle));
	}

	return scene_object_handle;
}

SceneObjectHandle SceneHierarchy::CreateSceneObjectAsLight(const CreateLightInfo& a_light_create_info, const char* a_name, const SceneObjectHandle a_parent)
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
	scene_object.mesh_handle = MeshHandle(BB_INVALID_HANDLE_64);
	scene_object.start_index = 0;
	scene_object.index_count = 0;
	scene_object.material = MaterialHandle(BB_INVALID_HANDLE_64);
	
	scene_object.transform = m_transform_pool.CreateTransform(a_light_create_info.pos);
	scene_object.child_count = 0;

	{
		Light light;
		light.light_type = static_cast<uint32_t>(a_light_create_info.light_type);
		light.color = a_light_create_info.color;
		light.pos = a_light_create_info.pos;

		light.specular_strength = a_light_create_info.specular_strength;
		light.radius_constant = a_light_create_info.radius_constant;
		light.radius_linear = a_light_create_info.radius_linear;
		light.radius_quadratic = a_light_create_info.radius_quadratic;

		light.spotlight_direction = a_light_create_info.spotlight_direction;
		light.cutoff_radius = a_light_create_info.cutoff_radius;

		scene_object.light_handle = m_light_container.insert(light);
	}

	return scene_object_handle;
}

void SceneHierarchy::SetView(const float4x4& a_view)
{
	m_per_frame.scene_info.view = a_view;
}

void SceneHierarchy::SetProjection(const float4x4& a_projection)
{
	m_per_frame.scene_info.proj = a_projection;
}

void SceneHierarchy::DrawSceneHierarchy(MemoryArena& a_temp_arena, const RCommandList a_list, const RenderTarget a_render_target, const uint2 a_draw_area_size, const int2 a_draw_area_offset)
{
	{	// RenderScenePerDraw(a_list, m_render_scene, a_render_target, a_draw_area_size, a_draw_area_offset, Slice(m_skybox_shaders, _countof(m_skybox_shaders)));

		RPipelineLayout pipe_layout = BindShaders(a_temp_arena, a_list, Slice(m_skybox_shaders, _countof(m_skybox_shaders)));

		// set 0
		{
			const uint32_t buffer_indices[] = { 0, 0 };
			const DescriptorAllocation& global_desc_alloc = GetGlobalDescriptorAllocation();
			const size_t buffer_offsets[]{ global_desc_alloc.offset, m_scene_descriptor.offset };
			//set 1-2
			SetDescriptorBufferOffset(a_list,
				pipe_layout,
				SPACE_GLOBAL,
				_countof(buffer_offsets),
				buffer_indices,
				buffer_offsets);
		}

		RenderingAttachmentColor color_attach{};
		color_attach.load_color = false;
		color_attach.store_color = true;
		color_attach.image_layout = IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL;
		color_attach.image_view = render_target.texture_info.view;

		StartRenderingInfo start_rendering_info;
		start_rendering_info.render_area_extent = a_draw_area_size;
		start_rendering_info.render_area_offset = a_draw_area_offset;
		start_rendering_info.color_attachments = Slice(&color_attach, 1);
		start_rendering_info.depth_attachment = nullptr;
		StartRenderPass(a_list, start_rendering_info);
		SetFrontFace(a_list, false);
		SetCullMode(a_list, CULL_MODE::NONE);

		DrawCubemap(a_list, 1, 0);

		EndRenderPass(a_list);
	}
	m_draw_list.size = 0;

	for (size_t i = 0; i < m_top_level_object_count; i++)
	{
		// identity hack to awkwardly get the first matrix. 
		DrawSceneObject(m_top_level_objects[i], Float4x4Identity());
	}

	// WAIT TO SEE IF WE CAN ACTUALLY DO COMMANDS

	{	// EndRenderScene(a_list, m_render_scene, a_render_target, a_draw_area_size, a_draw_area_offset);
		m_per_frame.scene_info.light_count = m_light_container.size();
		m_per_frame.scene_info.scene_resolution = a_draw_area_size;

		const size_t scene_upload_size = sizeof(Scene3DInfo);
		const size_t matrices_upload_size = sizeof(ShaderTransform) * m_draw_list.size;
		const size_t light_upload_size = sizeof(Light) * m_light_container.size();
		// optimize this
		const size_t total_size = scene_upload_size + matrices_upload_size + light_upload_size;

		const UploadBuffer upload_buffer = m_per_frame.frame_allocator.AllocateUploadMemory(total_size, m_per_frame.fence_value + 1);

		size_t bytes_uploaded = 0;
		upload_buffer.SafeMemcpy(bytes_uploaded, &m_per_frame.scene_info, scene_upload_size);
		const size_t scene_offset = bytes_uploaded + upload_buffer.base_offset;
		bytes_uploaded += scene_upload_size;

		upload_buffer.SafeMemcpy(bytes_uploaded, m_draw_list.transform, matrices_upload_size);
		const size_t matrix_offset = bytes_uploaded + upload_buffer.base_offset;
		bytes_uploaded += matrices_upload_size;

		upload_buffer.SafeMemcpy(bytes_uploaded, m_light_container.data(), light_upload_size);
		const size_t light_offset = bytes_uploaded + upload_buffer.base_offset;
		bytes_uploaded += light_upload_size;

		const RenderIO render_io = GetRenderIO();
		GPULinearBuffer& cur_scene_buffer = m_per_frame.uniform_buffer[render_io.frame_index];
		GPUBufferView scene_view;
		BB_ASSERT(cur_scene_buffer.Allocate(scene_upload_size, scene_view), "failed to allocate frame memory");
		GPUBufferView transform_view;
		BB_ASSERT(cur_scene_buffer.Allocate(matrices_upload_size, transform_view), "failed to allocate frame memory");
		GPUBufferView light_view;
		BB_ASSERT(cur_scene_buffer.Allocate(light_upload_size, light_view), "failed to allocate frame memory");

		//upload to some GPU buffer here.
		RenderCopyBuffer matrix_buffer_copy;
		matrix_buffer_copy.src = upload_buffer.buffer;
		matrix_buffer_copy.dst = cur_scene_buffer.GetBuffer();
		RenderCopyBufferRegion buffer_regions[3]; // 0 = scene, 1 = matrix
		buffer_regions[0].src_offset = scene_offset;
		buffer_regions[0].dst_offset = scene_view.offset;
		buffer_regions[0].size = scene_upload_size;

		buffer_regions[1].src_offset = matrix_offset;
		buffer_regions[1].dst_offset = transform_view.offset;
		buffer_regions[1].size = matrices_upload_size;

		buffer_regions[2].src_offset = light_offset;
		buffer_regions[2].dst_offset = light_view.offset;
		buffer_regions[2].size = light_upload_size;
		matrix_buffer_copy.regions = Slice(buffer_regions, _countof(buffer_regions));
		CopyBuffer(a_list, matrix_buffer_copy);
	}

	// WRITE DESCRIPTOR HERE


}

void SceneHierarchy::DrawSceneObject(const SceneObjectHandle a_scene_object, const float4x4& a_transform)
{
	const SceneObject& scene_object = m_scene_objects.find(a_scene_object);

	const float4x4 local_transform = a_transform * m_transform_pool.GetTransformMatrix(scene_object.transform);

	if (scene_object.mesh_handle.IsValid())
		AddToDrawList(scene_object, local_transform);

	for (size_t i = 0; i < scene_object.child_count; i++)
	{
		DrawSceneObject(scene_object.childeren[i], local_transform);
	}
}

void SceneHierarchy::AddToDrawList(const SceneObject& scene_object, const float4x4& a_transform)
{
	BB_ASSERT(m_draw_list.size + 1 >= m_draw_list.max_size, "too many drawn elements!");
	m_draw_list.mesh_draw_call[m_draw_list.size].mesh = scene_object.mesh_handle;
	m_draw_list.mesh_draw_call[m_draw_list.size].material = scene_object.material;
	m_draw_list.mesh_draw_call[m_draw_list.size].index_start = scene_object.start_index;
	m_draw_list.mesh_draw_call[m_draw_list.size].index_count = scene_object.index_count;
	m_draw_list.mesh_draw_call[m_draw_list.size].base_texture = scene_object.albedo_texture;
	m_draw_list.mesh_draw_call[m_draw_list.size].normal_texture = scene_object.normal_texture;
	m_draw_list.transform[m_draw_list.size].transform = a_transform;
	m_draw_list.transform[m_draw_list.size++].inverse = Float4x4Inverse(a_transform);
}
