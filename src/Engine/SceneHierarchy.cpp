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

	SceneCreateInfo create_info;
	create_info.ambient_light_color = float3(1.f, 1.f, 1.f);
	create_info.ambient_light_strength = 1;
	create_info.draw_entry_max = a_scene_obj_max;
	create_info.light_max = 128; // magic number jank yes shoot me

	m_render_scene = Create3DRenderScene(a_arena, create_info, a_name.c_str());
	const StringView view = StringView(a_name.c_str(), a_name.size());
	new (&m_scene_name) StringView(view);
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

		light_info.linear_distance = light_obj.Find("linear")->GetNumber();
		light_info.quadratic_distance = light_obj.Find("quadratic")->GetNumber();

		const StringView light_name = Asset::FindOrCreateString(light_obj.Find("name")->GetString());
		CreateSceneObjectAsLight(light_info, light_name.c_str());
	}

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
	scene_object.light_handle = CreateLight(m_render_scene, a_light_create_info);
	scene_object.transform = m_transform_pool.CreateTransform(a_light_create_info.pos);
	scene_object.child_count = 0;

	return scene_object_handle;
}

void SceneHierarchy::SetView(const float4x4& a_view)
{
	::SetView(m_render_scene, a_view);
}

void SceneHierarchy::SetProjection(const float4x4& a_projection)
{
	::SetProjection(m_render_scene, a_projection);
}

void SceneHierarchy::DrawSceneHierarchy(const RCommandList a_list, const RenderTarget a_render_target, const uint2 a_draw_area_size, const int2 a_draw_area_offset) const
{
	StartRenderScene(m_render_scene);
	for (size_t i = 0; i < m_top_level_object_count; i++)
	{
		// identity hack to awkwardly get the first matrix. 
		DrawSceneObject(m_top_level_objects[i], Float4x4Identity());
	}
	EndRenderScene(a_list, m_render_scene, a_render_target, a_draw_area_size, a_draw_area_offset, m_clear_color);
}

void SceneHierarchy::DrawSceneObject(const SceneObjectHandle a_scene_object, const float4x4& a_transform) const
{
	const SceneObject& scene_object = m_scene_objects.find(a_scene_object);

	const float4x4 local_transform = a_transform * m_transform_pool.GetTransformMatrix(scene_object.transform);

	if (scene_object.mesh_handle.IsValid())
		DrawMesh(m_render_scene, scene_object.mesh_handle, local_transform, scene_object.start_index, scene_object.index_count, scene_object.albedo_texture, scene_object.normal_texture, scene_object.material);

	for (size_t i = 0; i < scene_object.child_count; i++)
	{
		DrawSceneObject(scene_object.childeren[i], local_transform);
	}

}
