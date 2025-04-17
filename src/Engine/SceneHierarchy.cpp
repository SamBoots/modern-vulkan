#include "SceneHierarchy.hpp"
#include "Math.inl"
#include "BBjson.hpp"
#include "BBThreadScheduler.hpp"
#include "RendererTypes.hpp"
#include "OS/Program.h"
#include "MaterialSystem.hpp"
#include "ViewportInterface.hpp"

#include <vector>

using namespace BB;

// arbitrary, but just for a stack const char array
constexpr size_t UNIQUE_MODELS_PER_SCENE = 128;

constexpr uint32_t LIGHT_COUNT = 128;

void SceneHierarchy::Init(MemoryArena& a_arena, const uint32_t a_ecs_obj_max, const uint2 a_window_size, const uint32_t a_back_buffers, const StackString<32> a_name)
{
	{	// ECS systems
		EntityComponentSystemCreateInfo create_info;
		create_info.window_size = a_window_size;
		create_info.render_frame_count = a_back_buffers;

		create_info.entity_count = a_ecs_obj_max;
		create_info.light_count = LIGHT_COUNT;
		create_info.render_mesh_count = a_ecs_obj_max;
		m_ecs.Init(a_arena, create_info, a_name);
	}
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

SceneFrame SceneHierarchy::UpdateScene(const RCommandList a_list, Viewport& a_viewport)
{
	RenderSystem& render_sys = m_ecs.GetRenderSystem();
	SceneFrame scene_frame;

	m_ecs.TransformSystemUpdate();

	if (render_sys.GetRenderTargetExtent() != a_viewport.GetExtent())
	{
		render_sys.Resize(a_viewport.GetExtent());
		render_sys.SetProjection(a_viewport.CreateProjection(60.f, 0.001f, 10000.0f));
	}

	m_ecs.StartFrame();
	scene_frame.render_frame = m_ecs.RenderSystemUpdate(a_list, a_viewport.GetExtent());
	m_ecs.EndFrame();

	return scene_frame;
}

ECSEntity SceneHierarchy::CreateEntityViaModelNode(MemoryArena& a_temp_arena, const Model::Node& a_node, const ECSEntity a_parent)
{
	const ECSEntity ecs_obj = m_ecs.CreateEntity(a_node.name, a_parent, a_node.translation, a_node.rotation, a_node.scale);

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
            success = CreateRaytraceComponent(a_temp_arena, prim_obj, mesh_info);
            BB_ASSERT(success, "failed to create RaytraceComponent");
		}
	}

	for (uint32_t i = 0; i < a_node.child_count; i++)
	{
		CreateEntityViaModelNode(a_temp_arena, a_node.childeren[i], ecs_obj);
	}

	return ecs_obj;
}

bool SceneHierarchy::CreateRaytraceComponent(MemoryArena& a_temp_arena, const ECSEntity a_entity, const RenderComponent& a_render)
{
    AccelerationStructGeometrySize geometry_size{};
    geometry_size.vertex_count = a_render.index_count * 3;
    geometry_size.vertex_stride = sizeof(float3);
    const uint32_t max_primitives = a_render.index_count / 3;
    AccelerationStructSizeInfo sizes = GetBottomLevelAccelerationStructSizeInfo(a_temp_arena, ConstSlice<AccelerationStructGeometrySize>(&geometry_size, 1), ConstSlice<uint32_t>(&max_primitives, 1));
    
    RaytraceComponent component;
    component.build_size =  static_cast<uint32_t>(RoundUp(sizes.acceleration_structure_size, 256)); // remove magic number :)
    component.scratch_size = sizes.scratch_build_size;
    component.scratch_update = sizes.scratch_update_size;
    component.needs_build = true;
    component.needs_rebuild = false;

    bool success = m_ecs.EntityAssignRaytraceComponent(a_entity, component);
	BB_ASSERT(success, "failed to create RenderComponent");
    return true;
}

ECSEntity SceneHierarchy::CreateEntity(const float3 a_position, const NameComponent& a_name, const ECSEntity a_parent)
{
	return m_ecs.CreateEntity(a_name, a_parent, a_position);
}

ECSEntity SceneHierarchy::CreateEntityMesh(MemoryArena& a_temp_arena, const float3 a_position, const SceneMeshCreateInfo& a_mesh_info, const char* a_name, const ECSEntity a_parent)
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
    success = CreateRaytraceComponent(a_temp_arena, ecs_obj, mesh_info);
    BB_ASSERT(success, "failed to create RaytraceComponent");
	return ecs_obj;
}

ECSEntity SceneHierarchy::CreateEntityViaModel(MemoryArena& a_temp_arena, const Model& a_model, const float3 a_position, const char* a_name, const ECSEntity a_parent)
{
	const ECSEntity ecs_obj = m_ecs.CreateEntity(a_name, a_parent, a_position);

	for (uint32_t i = 0; i < a_model.root_node_count; i++)
	{
		CreateEntityViaModelNode(a_temp_arena, a_model.linear_nodes[a_model.root_node_indices[i]], ecs_obj);
	}

	return ecs_obj;
}

ECSEntity SceneHierarchy::CreateEntityAsLight(const LightCreateInfo& a_light_create_info, const char* a_name, const ECSEntity a_parent)
{
	const ECSEntity ecs_obj = m_ecs.CreateEntity(a_name, a_parent, a_light_create_info.pos);
	bool success = CreateLight(ecs_obj, a_light_create_info);
	BB_ASSERT(success, "failed to create light!");

	return ecs_obj;
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
	return m_ecs.EntityAssignLight(a_entity, light_component);
}

float4x4 SceneHierarchy::CalculateLightProjectionView(const float3 a_pos, const float a_near, const float a_far)
{
	const float4x4 projection = Float4x4Perspective(ToRadians(45.f), 1.0f, a_near, a_far);
	const float4x4 view = Float4x4Lookat(a_pos, float3(), float3(0.0f, -1.0f, 0.0f));
	return projection * view;
}
