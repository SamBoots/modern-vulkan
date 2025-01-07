#include "EntityComponentSystem.hpp"
#include "Renderer.hpp"
#include "MaterialSystem.hpp"

using namespace BB;

bool EntityComponentSystem::Init(MemoryArena& a_arena, const EntityComponentSystemCreateInfo& a_create_info)
{
	// components
	m_ecs_entities.Init(a_arena, a_create_info.entity_count);
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
}

ECSEntity EntityComponentSystem::CreateEntity(const NameComponent& a_name, const ECSEntity& a_parent, const float3 a_position, const float3x3 a_rotation, const float3 a_scale)
{
	ECSEntity entity;
	m_ecs_entities.CreateEntity(entity, a_parent);

	bool success = m_name_pool.CreateComponent(entity, a_name);
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

void EntityComponentSystem::TransformSystemUpdate()
{
	while (m_transform_system.dirty_transforms.Size() == 0)
	{
		const ECSEntity entity = m_transform_system.dirty_transforms[0];
		UpdateTransform(entity);
	}
}

void EntityComponentSystem::RenderSystemUpdate(const RCommandList a_list, const uint32_t a_back_buffer_index)
{

}

float3 EntityComponentSystem::Translate(const ECSEntity a_entity, const float3 a_translate)
{
	float3& pos = m_positions.GetComponent(a_entity);
	return pos = pos + a_translate;
}

float3x3 EntityComponentSystem::Rotate(const ECSEntity a_entity, const float3x3 a_rotate)
{
	float3x3& rot = m_rotations.GetComponent(a_entity);
	return rot = rot * a_rotate;
}

float3 EntityComponentSystem::Scale(const ECSEntity a_entity, const float3 a_scale)
{
	float3& scale = m_scales.GetComponent(a_entity);
	return scale = scale + a_scale;
}

void EntityComponentSystem::SetPosition(const ECSEntity a_entity, const float3 a_position)
{
	m_positions.GetComponent(a_entity) = a_position;
}

void EntityComponentSystem::SetRotation(const ECSEntity a_entity, const float3x3 a_rotation)
{
	m_rotations.GetComponent(a_entity) = a_rotation;
}

void EntityComponentSystem::SetScale(const ECSEntity a_entity, const float3 a_scale)
{
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

void EntityComponentSystem::UpdateTransform(const ECSEntity a_entity)
{
	float4x4& local_matrix = m_local_matrices.GetComponent(a_entity);
	local_matrix = Float4x4FromTranslation(m_positions.GetComponent(a_entity)) * m_rotations.GetComponent(a_entity);
	local_matrix = Float4x4Scale(local_matrix, m_scales.GetComponent(a_entity));

	const ECSEntity parent = m_ecs_entities.GetParent(a_entity);
	if (parent.IsValid())
	{
		if (m_transform_system.dirty_transforms.Find(parent.index) != INVALID_ECS_OBJ)
			UpdateTransform(parent);

		const float4x4& world_parent_matrix = m_world_matrices.GetComponent(parent);
		m_world_matrices.GetComponent(a_entity) = world_parent_matrix * local_matrix;
	}

	m_transform_system.dirty_transforms.Erase(a_entity);
}
