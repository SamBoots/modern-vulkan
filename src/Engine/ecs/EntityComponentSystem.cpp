#include "EntityComponentSystem.hpp"
#include "Renderer.hpp"
#include "MaterialSystem.hpp"

#include "Profiler.hpp"

#include "Math/Math.inl"
#include "Math/Collision.inl"

using namespace BB;

constexpr ECSSignatureIndex SIGNATURES[] =
{
    RELATION_ECS_SIGNATURE,
    NAME_ECS_SIGNATURE,
    POSITION_ECS_SIGNATURE,
    ROTATION_ECS_SIGNATURE,
    SCALE_ECS_SIGNATURE,
    LOCAL_MATRIX_ECS_SIGNATURE,
    WORLD_MATRIX_ECS_SIGNATURE
};

bool EntityComponentSystem::Init(MemoryArena& a_arena, const EntityComponentSystemCreateInfo& a_create_info, const StackString<32> a_name)
{
	m_name = a_name;

    m_frame_count = a_create_info.render_frame_count;
    m_per_frame_arena = MemoryArenaCreate();

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

    RenderOptions options;
    options.resolution = RENDER_OPTIONS::RESOLUTIONS[1];
    options.shadow_map_resolution = RENDER_OPTIONS::SHADOW_MAP_RESOLUTION[1];
    options.bloom_downscale_factor = RENDER_OPTIONS::BLOOM_DOWNSCALE_FACTOR[1];
    options.bloom_smoothness = RENDER_OPTIONS::BLOOM_SMOOTHNESS[1];
    options.backbuffer_format = IMAGE_FORMAT::RGBA8_SRGB;
    options.triple_buffering = true;
	m_render_system.Init(a_arena, options);

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

	success = m_ecs_entities.RegisterSignatures(entity, ConstSlice<const ECSSignatureIndex>(SIGNATURES, _countof(SIGNATURES)));
	BB_ASSERT(success, "ecs entity was not correctly deleted");

	return entity;
}

void EntityComponentSystem::FindECSEntityClickTraverse(const ECSEntity a_entity, const float3& a_ray_origin, const float3& a_ray_dir, ECSEntity& a_found, float& a_found_dist)
{
    if (m_ecs_entities.HasSignature(a_entity, BOUNDING_BOX_ECS_SIGNATURE)) // intersects
    {
        const float4x4& world_mat = m_world_matrices.GetComponent(a_entity);
        const BoundingBox box = m_bounding_box_pool.GetComponent(a_entity);

        const float4 p0 = (world_mat * float4(box.min.x, box.min.y, box.min.z, 1.0));
        const float4 p1 = (world_mat * float4(box.max.x, box.max.y, box.max.z, 1.0));
        float distance;
        if (BoxRayIntersectLength(float3(p0.x, p0.y, p0.z), float3(p1.x, p1.y, p1.z), a_ray_origin, a_ray_dir, distance))
        {   
            if (a_found_dist > distance)
            {
                a_found_dist = distance;
                a_found = a_entity;
            }
        }
    }

    const EntityRelation relation = m_relations.GetComponent(a_entity);

    if (relation.child_count == 0)
        return;

    ECSEntity child = relation.first_child;
    for (size_t i = 0; i < relation.child_count; i++)
    {
        FindECSEntityClickTraverse(child, a_ray_origin, a_ray_dir, a_found, a_found_dist);
        const EntityRelation child_relation = m_relations.GetComponent(child);
        child = child_relation.next;
    }

    return;
}

ECSEntity EntityComponentSystem::SelectEntityByRay(const float3 a_ray_origin, const float3 a_ray_dir)
{
    ECSEntity found = ECSEntity(INVALID_ECS_OBJ);
    float distance = FLT_MAX;
    for (size_t i = 0; i < m_root_entity_system.root_entities.GetDense().size(); i++)
    {
        FindECSEntityClickTraverse(m_root_entity_system.root_entities.GetDense()[i], a_ray_origin, a_ray_dir, found, distance);
    }

    return found;
}

bool EntityComponentSystem::DestroyEntity(const ECSEntity a_entity)
{
    if (!ValidateEntity(a_entity))
        return false;

    const EntityRelation relation = m_relations.GetComponent(a_entity);
    ECSEntity child = relation.first_child;
    for (size_t i = 0; i < relation.child_count; i++)
    {
        const EntityRelation child_relation = m_relations.GetComponent(child);
        const ECSEntity next_child = child_relation.next;
        DestroyEntity(child);
        child = next_child;
    }

    if (m_ecs_entities.HasSignature(a_entity, RELATION_ECS_SIGNATURE))
        m_relations.FreeComponent(a_entity);
    if (m_ecs_entities.HasSignature(a_entity, NAME_ECS_SIGNATURE))
        m_name_pool.FreeComponent(a_entity);
    if (m_ecs_entities.HasSignature(a_entity, POSITION_ECS_SIGNATURE))
        m_positions.FreeComponent(a_entity);
    if (m_ecs_entities.HasSignature(a_entity, ROTATION_ECS_SIGNATURE))
        m_rotations.FreeComponent(a_entity);
    if (m_ecs_entities.HasSignature(a_entity, SCALE_ECS_SIGNATURE))
        m_scales.FreeComponent(a_entity);
    if (m_ecs_entities.HasSignature(a_entity, LOCAL_MATRIX_ECS_SIGNATURE))
        m_local_matrices.FreeComponent(a_entity);
    if (m_ecs_entities.HasSignature(a_entity, WORLD_MATRIX_ECS_SIGNATURE))
        m_world_matrices.FreeComponent(a_entity);

    if (m_ecs_entities.HasSignature(a_entity, RENDER_ECS_SIGNATURE))
        m_render_mesh_pool.FreeComponent(a_entity);
    if (m_ecs_entities.HasSignature(a_entity, LIGHT_ECS_SIGNATURE))
        m_light_pool.FreeComponent(a_entity);
    if (m_ecs_entities.HasSignature(a_entity, RAYTRACE_ECS_SIGNATURE))
        m_raytrace_pool.FreeComponent(a_entity);
    if (m_ecs_entities.HasSignature(a_entity, BOUNDING_BOX_ECS_SIGNATURE))
        m_bounding_box_pool.FreeComponent(a_entity);

    if (m_root_entity_system.root_entities.Find(a_entity.index) != SPARSE_SET_INVALID)
        m_root_entity_system.root_entities.Erase(a_entity);

    
    return m_ecs_entities.FreeEntity(a_entity);
}

void EntityComponentSystem::AddLinesToFrame(const ConstSlice<Line> a_lines)
{
    //m_render_system.m_line_stage.AddLines(m_current_frame, a_lines);
}

void EntityComponentSystem::DrawAABB(const ECSEntity a_entity, const LineColor a_color)
{
    const float4x4& world_mat = m_world_matrices.GetComponent(a_entity);
    const BoundingBox box = m_bounding_box_pool.GetComponent(a_entity);

    const float4 p0 = (world_mat * float4(box.min.x, box.min.y, box.min.z, 1.0));
    const float4 p1 = (world_mat * float4(box.max.x, box.max.y, box.max.z, 1.0));
    const float3 world_p0 = float3(p0.x, p0.y, p0.z);
    const float3 world_p1 = float3(p1.x, p1.y, p1.z);

    FixedArray<Line, 12> lines{};
    Line line;
    line.p0_color = a_color;
    line.p1_color = a_color;
    // min
    line.p0 = world_p0;
    line.p1 = float3(world_p1.x, world_p0.y, world_p0.z);
    lines[0] = line;
    line.p1 = float3(world_p0.x, world_p1.y, world_p0.z);
    lines[1] = line;
    line.p1 = float3(world_p0.x, world_p0.y, world_p1.z);
    lines[2] = line;

    // max
    line.p0 = world_p1;
    line.p1 = float3(world_p0.x, world_p1.y, world_p1.z);
    lines[3] = line;
    line.p1 = float3(world_p1.x, world_p0.y, world_p1.z);
    lines[4] = line;
    line.p1 = float3(world_p1.x, world_p1.y, world_p0.z);
    lines[5] = line;

    // top min
    line.p0 = float3(world_p0.x, world_p1.y, world_p0.z);
    line.p1 = float3(world_p0.x, world_p1.y, world_p1.z);
    lines[6] = line;
    line.p1 = float3(world_p1.x, world_p1.y, world_p0.z);
    lines[7] = line;

    // bot max
    line.p0 = float3(world_p1.x, world_p0.y, world_p1.z);
    line.p1 = float3(world_p1.x, world_p0.y, world_p0.z);
    lines[8] = line;
    line.p1 = float3(world_p0.x, world_p0.y, world_p1.z);
    lines[9] = line;

    // sides
    line.p0 = float3(world_p0.x, world_p1.y, world_p1.z);
    line.p1 = float3(world_p0.x, world_p0.y, world_p1.z);
    lines[10] = line;
    line.p0 = float3(world_p1.x, world_p1.y, world_p0.z);
    line.p1 = float3(world_p1.x, world_p0.y, world_p0.z);
    lines[11] = line;
    AddLinesToFrame(lines.const_slice());
}

void EntityComponentSystem::StartFrame()
{
	MemoryArenaReset(m_per_frame_arena);
    m_render_system.StartFrame(m_per_frame_arena);
}

void EntityComponentSystem::EndFrame()
{
	m_current_frame = (m_current_frame + 1) % m_frame_count;
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
	StackString<32> rendering_name = m_name;
	rendering_name.append(" - render");
	BB_START_PROFILE(rendering_name);
	m_render_system.UpdateRenderSystem(m_per_frame_arena, a_list, a_draw_area_size, m_world_matrices, m_render_mesh_pool, m_raytrace_pool, m_light_pool.GetAllComponents());
	BB_END_PROFILE(rendering_name);

    //m_render_system.DebugDraw(a_list, a_draw_area_size);

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

float3 EntityComponentSystem::GetPosition(const ECSEntity a_entity) const
{
    return m_positions.GetComponent(a_entity);
}

float3x3 EntityComponentSystem::GetRotation(const ECSEntity a_entity) const
{
    return m_rotations.GetComponent(a_entity);
}

float3 EntityComponentSystem::GetScale(const ECSEntity a_entity) const
{
    return m_positions.GetComponent(a_entity);
}

bool EntityComponentSystem::ValidateEntity(const ECSEntity a_entity) const
{
    return m_ecs_entities.ValidateEntity(a_entity);
}

const float4x4& EntityComponentSystem::GetWorldMatrix(const ECSEntity a_entity) const
{
    BB_ASSERT(a_entity.IsValid(), "invalid entity");
    return m_world_matrices.GetComponent(a_entity);
}

const BoundingBox& EntityComponentSystem::GetBoundingBox(const ECSEntity a_entity) const
{
    BB_ASSERT(a_entity.IsValid(), "invalid entity");
    return m_bounding_box_pool.GetComponent(a_entity);
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

void EntityComponentSystem::CalculateView(const float3 a_pos, const float3 a_center, const float3 a_up)
{
    m_render_system.SetView(Float4x4Lookat(a_pos, a_center, a_up), a_pos);
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
