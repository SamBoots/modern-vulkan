#include "SceneHierarchy.hpp"
#include "Math.inl"
#include "BBjson.hpp"
#include "BBThreadScheduler.hpp"

#include "imgui.h"

#include <vector>

using namespace BB;

// arbritrary, but just for a stack const char array
constexpr size_t UNIQUE_MODELS_PER_SCENE = 128;

void SceneHierarchy::Init(MemoryArena& a_memory_arena, const StringView a_name, const uint32_t a_scene_obj_max)
{
	m_transform_pool.Init(a_memory_arena, a_scene_obj_max);
	m_scene_objects.Init(a_memory_arena, a_scene_obj_max);
	m_top_level_objects = ArenaAllocArr(a_memory_arena, SceneObjectHandle, a_scene_obj_max);

	m_top_level_object_count = 0;

	SceneCreateInfo create_info;
	create_info.ambient_light_color = float3(1.f, 1.f, 1.f);
	create_info.ambient_light_strength = 1;
	create_info.draw_entry_max = a_scene_obj_max;
	create_info.light_max = 128; // magic number jank yes shoot me

	m_render_scene = Create3DRenderScene(a_memory_arena, create_info, a_name.c_str());
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

void BB::SceneHierarchy::InitViaJson(MemoryArena& a_memory_arena, const FixedArray<ShaderEffectHandle, 2>& a_TEMP_shader_effects, const char* a_json_path, const uint32_t a_scene_obj_max)
{
	JsonParser scene_json(a_json_path);
	scene_json.Parse();

	// hell
	const JsonObject& scene_obj = scene_json.GetRootNode()->GetObject().Find("scene")->GetObject();

	{
		const JsonNode& name_node = *scene_obj.Find("name");
		const StringView scene_name = Asset::FindOrCreateString(name_node.GetString());
		Init(a_memory_arena, scene_name, a_scene_obj_max);
	}
	
	MemoryArenaScope(a_memory_arena)
	{
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

		Asset::AsyncAsset* async_model_loads = ArenaAllocArr(a_memory_arena, Asset::AsyncAsset, unique_model_count);

		for (size_t i = 0; i < unique_model_count; i++)
		{
			async_model_loads[i].asset_type = Asset::ASYNC_ASSET_TYPE::MODEL;
			async_model_loads[i].load_type = Asset::ASYNC_LOAD_TYPE::DISK;
			async_model_loads[i].mesh_disk.path = unique_models[i];
		}

		const ThreadTask asset_transfer_task = Asset::LoadAssetsASync(Slice(async_model_loads, unique_model_count), "upload scene models json");

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

		// not nice :(
		Threads::WaitForTask(asset_transfer_task);

		for (size_t i = 0; i < scene_objects.node_count; i++)
		{
			const JsonObject& sce_obj = scene_objects.nodes[i]->GetObject();
			const char* model_name = scene_objects.nodes[i]->GetObject().Find("file_name")->GetString();
			const char* obj_name = scene_objects.nodes[i]->GetObject().Find("file_name")->GetString();
			const Model* model = Asset::FindModelByPath(model_name);

			const JsonList& position_list = sce_obj.Find("position")->GetList();
			BB_ASSERT(position_list.node_count == 3, "scene_object position in scene json is not 3 elements");
			float3 position;
			position.x = position_list.nodes[0]->GetNumber();
			position.y = position_list.nodes[1]->GetNumber();
			position.z = position_list.nodes[2]->GetNumber();

			CreateSceneObjectViaModel(*model, a_TEMP_shader_effects, position, obj_name);
		}
	}
}

SceneObjectHandle SceneHierarchy::CreateSceneObjectViaModelNode(const Model& a_model, const FixedArray<ShaderEffectHandle, 2>& a_TEMP_shader_effects, const Model::Node& a_node, const SceneObjectHandle a_parent)
{
	//decompose the matrix.
	float3 transform;
	float3 scale;
	Quat rotation;
	Float4x4DecomposeTransform(a_node.transform, transform, rotation, scale);

	const SceneObjectHandle scene_handle = m_scene_objects.emplace(SceneObject());
	SceneObject& scene_obj = m_scene_objects.find(scene_handle);
	scene_obj.name = a_node.name;
	scene_obj.mesh_handle = MeshHandle(BB_INVALID_HANDLE_64);
	scene_obj.start_index = 0;
	scene_obj.index_count = 0;
	scene_obj.material = MaterialHandle(BB_INVALID_HANDLE_64);
	scene_obj.light_handle = LightHandle(BB_INVALID_HANDLE_64);
	scene_obj.transform = m_transform_pool.CreateTransform(transform, rotation, scale);
	scene_obj.parent = a_parent;

	if (a_node.mesh_handle.IsValid())
	{
		for (uint32_t i = 0; i < a_node.primitive_count; i++)
		{
			BB_ASSERT(scene_obj.child_count < SCENE_OBJ_CHILD_MAX, "Too many childeren for a single scene object!");
			SceneObject prim_obj{};
			prim_obj.name = a_node.primitives[i].name;
			prim_obj.mesh_handle = a_node.mesh_handle;
			prim_obj.start_index = a_node.primitives[i].start_index;
			prim_obj.index_count = a_node.primitives[i].index_count;
			CreateMaterialInfo mat_info;
			mat_info.base_color = a_node.primitives[i].material_info.base_texture;
			mat_info.normal_texture = a_node.primitives[i].material_info.normal_texture;
			mat_info.name = a_node.primitives[i].material_info.name;
			mat_info.shader_effects = Slice<const ShaderEffectHandle>(&a_TEMP_shader_effects.m_arr[0], 2);
			prim_obj.material = CreateMaterial(mat_info);

			scene_obj.light_handle = LightHandle(BB_INVALID_HANDLE_64);
			prim_obj.transform = m_transform_pool.CreateTransform(float3(0, 0, 0));

			prim_obj.parent = scene_handle;
			scene_obj.childeren[scene_obj.child_count++] = m_scene_objects.emplace(prim_obj);
		}
	}

	for (uint32_t i = 0; i < a_node.child_count; i++)
	{
		BB_ASSERT(scene_obj.child_count < SCENE_OBJ_CHILD_MAX, "Too many childeren for a single gameobject!");
		scene_obj.childeren[scene_obj.child_count++] = CreateSceneObjectViaModelNode(a_model, a_TEMP_shader_effects, a_node.childeren[i], a_parent);
	}

	return scene_handle;
}

void SceneHierarchy::CreateSceneObjectViaModel(const Model& a_model, const FixedArray<ShaderEffectHandle, 2>& a_TEMP_shader_effects, const float3 a_position, const char* a_name)
{
	SceneObjectHandle top_level_handle = m_scene_objects.emplace(SceneObject());
	SceneObject& top_level_object = m_scene_objects.find(top_level_handle);
	top_level_object.name = a_name;
	top_level_object.mesh_handle = MeshHandle(BB_INVALID_HANDLE_64);
	top_level_object.start_index = 0;
	top_level_object.index_count = 0;
	top_level_object.material = MaterialHandle(BB_INVALID_HANDLE_64);
	top_level_object.parent = SceneObjectHandle(BB_INVALID_HANDLE_64);
	top_level_object.light_handle = LightHandle(BB_INVALID_HANDLE_64);
	top_level_object.transform = m_transform_pool.CreateTransform(a_position);

	top_level_object.child_count = a_model.root_node_count;
	BB_ASSERT(top_level_object.child_count < SCENE_OBJ_CHILD_MAX, "Too many childeren for a single scene object!");

	for (uint32_t i = 0; i < a_model.root_node_count; i++)
	{
		top_level_object.childeren[i] = CreateSceneObjectViaModelNode(a_model, a_TEMP_shader_effects, a_model.root_nodes[i], top_level_handle);
	}

	BB_ASSERT(m_top_level_object_count < m_scene_objects.capacity(), "Too many scene objects, increase the max");
	m_top_level_objects[m_top_level_object_count++] = top_level_handle;
}

void SceneHierarchy::CreateSceneObjectAsLight(const CreateLightInfo& a_light_create_info, const char* a_name)
{
	const SceneObjectHandle scene_object_handle = m_scene_objects.emplace(SceneObject());
	SceneObject& scene_object_light = m_scene_objects.find(scene_object_handle);
	scene_object_light.name = a_name;
	scene_object_light.mesh_handle = MeshHandle(BB_INVALID_HANDLE_64);
	scene_object_light.start_index = 0;
	scene_object_light.index_count = 0;
	scene_object_light.material = MaterialHandle(BB_INVALID_HANDLE_64);
	scene_object_light.parent = SceneObjectHandle(BB_INVALID_HANDLE_64);
	scene_object_light.light_handle = CreateLight(m_render_scene, a_light_create_info);
	scene_object_light.transform = m_transform_pool.CreateTransform(a_light_create_info.pos);

	scene_object_light.child_count = 0;

	BB_ASSERT(m_top_level_object_count < m_scene_objects.capacity(), "Too many scene objects, increase the max");

	m_top_level_objects[m_top_level_object_count++] = scene_object_handle;
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

	if (scene_object.mesh_handle.handle != BB_INVALID_HANDLE_64)
		DrawMesh(m_render_scene, scene_object.mesh_handle, local_transform, scene_object.start_index, scene_object.index_count, scene_object.material);

	for (size_t i = 0; i < scene_object.child_count; i++)
	{
		DrawSceneObject(scene_object.childeren[i], local_transform);
	}

}

SceneObjectHandle SceneHierarchy::CreateSceneObjectEmpty(const SceneObjectHandle a_parent)
{
	SceneObjectHandle scene_obj_handle = m_scene_objects.emplace(SceneObject());
	SceneObject& scene_obj = m_scene_objects.find(scene_obj_handle);
	scene_obj.name = "default";
	scene_obj.mesh_handle = MeshHandle(BB_INVALID_HANDLE_64);
	scene_obj.start_index = 0;
	scene_obj.index_count = 0;
	scene_obj.material = MaterialHandle(BB_INVALID_HANDLE_64);
	scene_obj.parent = SceneObjectHandle(BB_INVALID_HANDLE_64);
	scene_obj.light_handle = LightHandle(BB_INVALID_HANDLE_64);
	scene_obj.transform = m_transform_pool.CreateTransform(float3(0.f, 0.f, 0.f));
	scene_obj.child_count = 0;

	scene_obj.parent = a_parent;

	return scene_obj_handle;
}

void SceneHierarchy::ImguiDisplaySceneHierarchy()
{
	if (ImGui::Begin(m_scene_name.c_str()))
	{
		ImGui::Indent();
		if (ImGui::Button("create scene object"))
		{
			BB_ASSERT(m_top_level_object_count <= m_scene_objects.capacity(), "Too many render object childeren for this object!");
			m_top_level_objects[m_top_level_object_count++] = CreateSceneObjectEmpty(SceneObjectHandle(BB_INVALID_HANDLE_64));
		}

		for (size_t i = 0; i < m_top_level_object_count; i++)
		{
			ImGui::PushID(static_cast<int>(i));

			ImGuiDisplaySceneObject(m_top_level_objects[i]);

			ImGui::PopID();
		}

		ImGui::Unindent();
	}
	ImGui::End();
}

void SceneHierarchy::ImGuiDisplaySceneObject(const SceneObjectHandle a_object)
{
	SceneObject& scene_object = m_scene_objects.find(a_object);
	ImGui::PushID(static_cast<int>(a_object.handle));

	if (ImGui::CollapsingHeader(scene_object.name))
	{
		ImGui::Indent();
		Transform& transform = m_transform_pool.GetTransform(scene_object.transform);

		bool position_changed = false;

		if (ImGui::TreeNodeEx("transform"))
		{
			if (ImGui::InputFloat3("position", transform.m_pos.e))
			{
				position_changed = true;
			}

			ImGui::InputFloat4("rotation quat (xyzw)", transform.m_rot.xyzw.e);
			ImGui::InputFloat3("scale", transform.m_scale.e);
			ImGui::TreePop();
		}

		if (scene_object.light_handle.IsValid())
		{
			if (ImGui::TreeNodeEx("light object"))
			{
				ImGui::Indent();
				PointLight& light = GetLight(m_render_scene, scene_object.light_handle);

				if (position_changed)
				{
					light.pos = transform.m_pos;
				}

				ImGui::InputFloat3("color", light.color.e);
				ImGui::InputFloat("linear radius", &light.radius_linear);
				ImGui::InputFloat("quadratic radius", &light.radius_quadratic);

				if (ImGui::Button("remove light"))
				{
					FreeLight(m_render_scene, scene_object.light_handle);
					scene_object.light_handle = LightHandle(BB_INVALID_HANDLE_64);
				}
				ImGui::Unindent();
				ImGui::TreePop();
			}
		}
		else
		{
			if (ImGui::Button("create light"))
			{
				CreateLightInfo light_create_info;
				light_create_info.pos = transform.m_pos;
				light_create_info.color = float3(1.f, 1.f, 1.f);
				light_create_info.linear_distance = 0.35f;
				light_create_info.quadratic_distance = 0.44f;
				scene_object.light_handle = CreateLight(m_render_scene, light_create_info);
			}
		}

		for (size_t i = 0; i < scene_object.child_count; i++)
		{
			ImGuiDisplaySceneObject(scene_object.childeren[i]);
		}

		if (ImGui::Button("create scene object"))
		{
			BB_ASSERT(scene_object.child_count <= SCENE_OBJ_CHILD_MAX, "Too many render object childeren for this object!");
			scene_object.childeren[scene_object.child_count++] = CreateSceneObjectEmpty(a_object);
		}

		ImGui::Unindent();
	}

	ImGui::PopID();
}
