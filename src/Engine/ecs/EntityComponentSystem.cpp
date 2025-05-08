#include "EntityComponentSystem.hpp"
#include "Renderer.hpp"
#include "MaterialSystem.hpp"

#include "Profiler.hpp"

#include "Math/Math.inl"
#include "Math/Collision.inl"

using namespace BB;

bool EntityComponentSystem::Init(MemoryArena& a_arena, const EntityComponentSystemCreateInfo& a_create_info, const StackString<32> a_name)
{
	m_name = a_name;

    m_lines.Init(a_arena, 200);
	m_per_frame.Init(a_arena, a_create_info.render_frame_count);
	m_per_frame.resize(a_create_info.render_frame_count);
	for (uint32_t i = 0; i < a_create_info.render_frame_count; i++)
	{
		m_per_frame[i].arena = MemoryArenaCreate();
	}

	// components
	m_ecs_entities.Init(a_arena, a_create_info.entity_count);
	m_relations.Init(a_arena, a_create_info.entity_count);
	m_name_pool.Init(a_arena, a_create_info.entity_count);
    m_bounding_box_pool.Init(a_arena, a_create_info.entity_count);
	m_positions.Init(a_arena, a_create_info.entity_count);
	m_rotations.Init(a_arena, a_create_info.entity_count);
	m_scales.Init(a_arena, a_create_info.entity_count);
	m_local_matrices.Init(a_arena, a_create_info.entity_count);
	m_world_matrices.Init(a_arena, a_create_info.entity_count);
	m_render_mesh_pool.Init(a_arena, a_create_info.render_mesh_count, a_create_info.entity_count);
	m_light_pool.Init(a_arena, a_create_info.light_count, a_create_info.entity_count);
    m_raytrace_pool.Init(a_arena, a_create_info.render_mesh_count, a_create_info.entity_count);

	// maybe better system for this?
	m_transform_system.dirty_transforms.Init(a_arena, a_create_info.entity_count, a_create_info.entity_count);
	m_root_entity_system.root_entities.Init(a_arena, a_create_info.entity_count, a_create_info.entity_count / 4);

	m_render_system.Init(a_arena, a_create_info.render_frame_count, a_create_info.light_count, a_create_info.window_size);

	return true;
}

ECSEntity EntityComponentSystem::CreateEntity(const NameComponent& a_name, const ECSEntity& a_parent, const float3 a_position, const float3x3 a_rotation, const float3 a_scale)
{
	ECSEntity entity;
	bool success = m_ecs_entities.CreateEntity(entity);
	BB_ASSERT(success, "error creating ecs entity");

	success = AddEntityRelation(entity, a_parent);
	BB_ASSERT(success, "ecs entity was not correctly deleted");
	success = m_name_pool.CreateComponent(entity, a_name);
	BB_ASSERT(success, "ecs entity was not correctly deleted");
	success = m_positions.CreateComponent(entity, a_position);
	BB_ASSERT(success, "ecs entity was not correctly deleted");
	success = m_rotations.CreateComponent(entity, a_rotation);
	BB_ASSERT(success, "ecs entity was not correctly deleted");
	success = m_scales.CreateComponent(entity, a_scale);
	BB_ASSERT(success, "ecs entity was not correctly deleted");
	success = m_local_matrices.CreateComponent(entity);
	BB_ASSERT(success, "ecs entity was not correctly deleted");
	success = m_world_matrices.CreateComponent(entity);
	BB_ASSERT(success, "ecs entity was not correctly deleted");
	// register to transform dirty system
	const uint32_t res = m_transform_system.dirty_transforms.Insert(entity);
	BB_ASSERT(res != SPARSE_SET_INVALID || res != SPARSE_SET_ALREADY_SET, "ecs entity can't be added to dirty transforms");

	constexpr ECSSignatureIndex signatures[] =
	{
		RELATION_ECS_SIGNATURE,
		NAME_ECS_SIGNATURE,
		POSITION_ECS_SIGNATURE,
		ROTATION_ECS_SIGNATURE,
		SCALE_ECS_SIGNATURE,
		LOCAL_MATRIX_ECS_SIGNATURE,
		WORLD_MATRIX_ECS_SIGNATURE
	};

	success = m_ecs_entities.RegisterSignatures(entity, ConstSlice<const ECSSignatureIndex>(signatures, _countof(signatures)));
	BB_ASSERT(success, "ecs entity was not correctly deleted");

	return entity;
}

void EntityComponentSystem::AddAABBBoxToLines(const float3 a_world_min, const float3 a_world_max)
{
    Line line;
    line.p0_color = Color{255, 0, 0, 255};
    line.p1_color = Color{255, 0, 0, 255};
    // min
    line.p0 = a_world_min;
    line.p1 = float3(a_world_max.x, a_world_min.y, a_world_min.z);
    m_lines.emplace_back(line);
    line.p1 = float3(a_world_min.x, a_world_max.y, a_world_min.z);
    m_lines.emplace_back(line);
    line.p1 = float3(a_world_min.x, a_world_min.y, a_world_max.z);
    m_lines.emplace_back(line);

    // max
    line.p0 = a_world_max;
    line.p1 = float3(a_world_min.x, a_world_max.y, a_world_max.z);
    m_lines.emplace_back(line);
    line.p1 = float3(a_world_max.x, a_world_min.y, a_world_max.z);
    m_lines.emplace_back(line);
    line.p1 = float3(a_world_max.x, a_world_max.y, a_world_min.z);
    m_lines.emplace_back(line);

    // top min
    line.p0 = float3(a_world_min.x, a_world_max.y, a_world_min.z);
    line.p1 = float3(a_world_min.x, a_world_max.y, a_world_max.z);
    m_lines.emplace_back(line);
    line.p1 = float3(a_world_max.x, a_world_max.y, a_world_min.z);
    m_lines.emplace_back(line);

    // bot max
    line.p0 = float3(a_world_max.x, a_world_min.y, a_world_max.z);
    line.p1 = float3(a_world_max.x, a_world_min.y, a_world_min.z);
    m_lines.emplace_back(line);
    line.p1 = float3(a_world_min.x, a_world_min.y, a_world_max.z);
    m_lines.emplace_back(line);

    // sides
    line.p0 = float3(a_world_min.x, a_world_max.y, a_world_max.z);
    line.p1 = float3(a_world_min.x, a_world_min.y, a_world_max.z);
    m_lines.emplace_back(line);
    line.p0 = float3(a_world_max.x, a_world_max.y, a_world_min.z);
    line.p1 = float3(a_world_max.x, a_world_min.y, a_world_min.z);
    m_lines.emplace_back(line);
}

ECSEntity EntityComponentSystem::FindECSEntityClickTraverse(const ECSEntity a_entity, const float3& a_ray_origin, const float3& a_ray_dir)
{
    if (m_ecs_entities.HasSignature(a_entity, BOUNDING_BOX_ECS_SIGNATURE)) // intersects
    {
        const float4x4& world_mat = m_world_matrices.GetComponent(a_entity);
        const BoundingBox box = m_bounding_box_pool.GetComponent(a_entity);

        const float4 p0 = (world_mat * float4(box.min.x, box.min.y, box.min.z, 1.0));
        const float4 p1 = (world_mat * float4(box.max.x, box.max.y, box.max.z, 1.0));
        if (BoxRayIntersect(float3(p0.x, p0.y, p0.z), float3(p1.x, p1.y, p1.z), a_ray_origin, a_ray_dir))
        {
            AddAABBBoxToLines(float3(p0.x, p0.y, p0.z), float3(p1.x, p1.y, p1.z));
            return a_entity;
        }
    }

    const EntityRelation relation = m_relations.GetComponent(a_entity);

    if (relation.child_count == 0)
        return INVALID_ECS_OBJ;

    ECSEntity child = relation.first_child;
    for (size_t i = 0; i < relation.child_count; i++)
    {
        const ECSEntity found_child = FindECSEntityClickTraverse(child, a_ray_origin, a_ray_dir);
        if (found_child != INVALID_ECS_OBJ)
            return found_child;
        const EntityRelation child_relation = m_relations.GetComponent(child);
        child = child_relation.next;
    }

    return INVALID_ECS_OBJ;
}

ECSEntity EntityComponentSystem::SelectEntityByClick(const float2 a_mouse_pos_viewport, const float4x4& a_view, const float3& a_ray_origin)
{
    const float3 ndc
    {
        (2.f * a_mouse_pos_viewport.x) / static_cast<float>(m_render_system.GetRenderTargetExtent().x) - 1.f,
        1.0f - (2.0f * (m_render_system.GetRenderTargetExtent().y -a_mouse_pos_viewport.y)) / static_cast<float>(m_render_system.GetRenderTargetExtent().y),
        1.f
    };

    const float4 ray_clip = float4(ndc.x, ndc.y, -1.f, 1.f);
    const float4 ray_inverse = Float4x4Inverse(m_render_system.GetProjection()) * ray_clip;
    const float4 ray_eye = float4(ray_inverse.x, ray_inverse.y, -1.0, 0.0f);
    const float4 ray_world = Float4x4Inverse(a_view) * ray_eye;
    const float3 ray_world_norm = Float3Normalize(float3(ray_world.x, ray_world.y, ray_world.z));

    Line line;
    line.p0_color = Color{0, 255, 0, 255};
    line.p1_color = Color{0, 255, 0, 255};
    line.p0 = a_ray_origin;
    line.p1 = ray_world_norm * 10 + a_ray_origin;
    m_lines.emplace_back(line);

    for (size_t i = 0; i < m_root_entity_system.root_entities.GetDense().size(); i++)
    {
        const ECSEntity found_child = FindECSEntityClickTraverse(m_root_entity_system.root_entities.GetDense()[i], a_ray_origin, ray_world_norm);
        if (found_child != INVALID_ECS_OBJ)
            return found_child;
    }

    return INVALID_ECS_OBJ;
}

void EntityComponentSystem::StartFrame()
{
	PerFrame& frame = m_per_frame[m_current_frame];
	MemoryArenaReset(frame.arena);
}

void EntityComponentSystem::EndFrame()
{
	m_current_frame = (m_current_frame + 1) % m_per_frame.size();
}

void EntityComponentSystem::TransformSystemUpdate()
{
	while (m_transform_system.dirty_transforms.Size() != 0)
	{
		UpdateTransform(m_transform_system.dirty_transforms[0]);
	}
}

RenderSystemFrame EntityComponentSystem::RenderSystemUpdate(const RCommandList a_list, const uint2 a_draw_area_size)
{
	m_render_system.StartFrame(a_list);

	StackString<32> rendering_name = m_name;
	rendering_name.append(" - render");
	BB_START_PROFILE(rendering_name);
	m_render_system.UpdateRenderSystem(m_per_frame[m_current_frame].arena, a_list, a_draw_area_size, m_world_matrices, m_render_mesh_pool, m_raytrace_pool, m_light_pool.GetAllComponents());
	BB_END_PROFILE(rendering_name);

    m_render_system.DebugDraw(a_list, a_draw_area_size, m_lines.const_slice());

	return m_render_system.EndFrame(a_list, IMAGE_LAYOUT::RT_COLOR);
}

float3 EntityComponentSystem::Translate(const ECSEntity a_entity, const float3 a_translate)
{
	m_transform_system.dirty_transforms.Insert(a_entity);
	float3& pos = m_positions.GetComponent(a_entity);
	return pos = pos + a_translate;
}

float3x3 EntityComponentSystem::Rotate(const ECSEntity a_entity, const float3x3 a_rotate)
{
	m_transform_system.dirty_transforms.Insert(a_entity);
	float3x3& rot = m_rotations.GetComponent(a_entity);
	return rot = rot * a_rotate;
}

float3 EntityComponentSystem::Scale(const ECSEntity a_entity, const float3 a_scale)
{
	m_transform_system.dirty_transforms.Insert(a_entity);
	float3& scale = m_scales.GetComponent(a_entity);
	return scale = scale + a_scale;
}

void EntityComponentSystem::SetPosition(const ECSEntity a_entity, const float3 a_position)
{
	m_transform_system.dirty_transforms.Insert(a_entity);
	m_positions.GetComponent(a_entity) = a_position;
}

void EntityComponentSystem::SetRotation(const ECSEntity a_entity, const float3x3 a_rotation)
{
	m_transform_system.dirty_transforms.Insert(a_entity);
	m_rotations.GetComponent(a_entity) = a_rotation;
}

void EntityComponentSystem::SetScale(const ECSEntity a_entity, const float3 a_scale)
{
	m_transform_system.dirty_transforms.Insert(a_entity);
	m_scales.GetComponent(a_entity) = a_scale;
}

bool EntityComponentSystem::EntityAssignBoundingBox(const ECSEntity a_entity, const BoundingBox& a_box)
{
    if (!m_bounding_box_pool.CreateComponent(a_entity, a_box))
        return false;
    if (!m_ecs_entities.RegisterSignature(a_entity, m_bounding_box_pool.GetSignatureIndex()))
        return false;
    return true;
}

bool EntityComponentSystem::EntityAssignName(const ECSEntity a_entity, const NameComponent& a_name)
{
	if (!m_name_pool.CreateComponent(a_entity, a_name))
		return false;
	if (!m_ecs_entities.RegisterSignature(a_entity, m_name_pool.GetSignatureIndex()))
		return false;
	return true;
}

bool EntityComponentSystem::EntityAssignRenderComponent(const ECSEntity a_entity, const RenderComponent& a_draw_info)
{
	if (!m_render_mesh_pool.CreateComponent(a_entity, a_draw_info))
		return false;
	if (!m_ecs_entities.RegisterSignature(a_entity, m_render_mesh_pool.GetSignatureIndex()))
		return false;
	return true;
}

bool EntityComponentSystem::EntityAssignLight(const ECSEntity a_entity, const LightComponent& a_light)
{
	if (!m_light_pool.CreateComponent(a_entity, a_light))
		return false;
	if (!m_ecs_entities.RegisterSignature(a_entity, m_light_pool.GetSignatureIndex()))
		return false;
	return true;
}

bool EntityComponentSystem::EntityAssignRaytraceComponent(const ECSEntity a_entity, const RaytraceComponent& a_raytrace)
{
    if (!m_raytrace_pool.CreateComponent(a_entity, a_raytrace))
		return false;
	if (!m_ecs_entities.RegisterSignature(a_entity, m_raytrace_pool.GetSignatureIndex()))
		return false;
	return true;
}

bool EntityComponentSystem::EntityFreeLight(const ECSEntity a_entity)
{
	if (!m_ecs_entities.HasSignature(a_entity, LIGHT_ECS_SIGNATURE))
		return false;
	if (!m_light_pool.FreeComponent(a_entity))
		return false;
	if (!m_ecs_entities.UnregisterSignature(a_entity, LIGHT_ECS_SIGNATURE))
		return false;
	return true;
}

bool EntityComponentSystem::AddEntityRelation(const ECSEntity a_entity, const ECSEntity a_parent)
{
	// TODO, maybe add previous entry as well.
	EntityRelation relations;
	relations.child_count = 0;
	relations.first_child = INVALID_ECS_OBJ;
	relations.next = INVALID_ECS_OBJ;
	relations.parent = a_parent;
	if (relations.parent.IsValid())
	{
		EntityRelation& parent_relation = m_relations.GetComponent(relations.parent);
		if (parent_relation.child_count != 0)
		{
			ECSEntity next_free = parent_relation.first_child;
			for (size_t i = 1; i < parent_relation.child_count; i++)
			{
				next_free = m_relations.GetComponent(next_free).next;
			}

			BB_ASSERT(!m_relations.GetComponent(next_free).next.IsValid(), "next free entity is not free!");
			m_relations.GetComponent(next_free).next = a_entity;
		}
		else
			parent_relation.first_child = a_entity;

		++parent_relation.child_count;
	}
	else
		m_root_entity_system.root_entities.Insert(a_entity);

	return m_relations.CreateComponent(a_entity, relations);
}

void EntityComponentSystem::UpdateTransform(const ECSEntity a_entity)
{
	float4x4& local_matrix = m_local_matrices.GetComponent(a_entity);
	local_matrix = Float4x4FromTranslation(m_positions.GetComponent(a_entity)) * m_rotations.GetComponent(a_entity);
	local_matrix = Float4x4Scale(local_matrix, m_scales.GetComponent(a_entity));

	const EntityRelation& parent_relation = m_relations.GetComponent(a_entity);
	if (parent_relation.parent.IsValid())
	{
		const float4x4& world_parent_matrix = m_world_matrices.GetComponent(parent_relation.parent);
		m_world_matrices.GetComponent(a_entity) = world_parent_matrix * local_matrix;
	}
	else
	{
		m_world_matrices.GetComponent(a_entity) = local_matrix;
	}

	m_transform_system.dirty_transforms.Erase(a_entity);

	// update all the chilren last
	ECSEntity child = parent_relation.first_child;
	for (size_t i = 0; i < parent_relation.child_count; i++)
	{
		UpdateTransform(child);
		child = m_relations.GetComponent(child).next;
	}
}
