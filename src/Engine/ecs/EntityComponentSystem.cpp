#include "EntityComponentSystem.hpp"
#include "Renderer.hpp"
#include "MaterialSystem.hpp"

#include "Profiler.hpp"

#include "Math.inl"

using namespace BB;

bool EntityComponentSystem::Init(MemoryArena& a_arena, const EntityComponentSystemCreateInfo& a_create_info, const StackString<32> a_name)
{
	m_name = a_name;


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
	m_positions.Init(a_arena, a_create_info.entity_count);
	m_rotations.Init(a_arena, a_create_info.entity_count);
	m_scales.Init(a_arena, a_create_info.entity_count);
	m_local_matrices.Init(a_arena, a_create_info.entity_count);
	m_world_matrices.Init(a_arena, a_create_info.entity_count);
	m_render_mesh_pool.Init(a_arena, a_create_info.render_mesh_count, a_create_info.entity_count);
	m_light_pool.Init(a_arena, a_create_info.light_count, a_create_info.entity_count);

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
	m_render_system.UpdateRenderSystem(m_per_frame[m_current_frame].arena, a_list, a_draw_area_size, m_world_matrices, m_render_mesh_pool, m_light_pool.GetAllComponents());
	BB_END_PROFILE(rendering_name);

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
