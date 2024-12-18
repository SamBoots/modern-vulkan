#include "TransformSystem.hpp"

using namespace BB;

bool TransformSystem::Init(MemoryArena& a_arena, const uint32_t a_entity_max)
{
	m_dirty_matrices.Init(a_arena, a_entity_max);
}

bool TransformSystem::EntityAddTransform(const ECSEntity a_entity, const float3 a_pos = float3(0.f), const float3x3 a_rotation = Float3x3Identity(), const float3 a_scale = float3(1.f))
{

}

float3 TransformSystem::Translate(const ECSEntity a_entity, const float3 a_translate)
{
	m_pos = m_pos + a_translation;
}

float3x3 TransformSystem::Rotate(const ECSEntity a_entity, const float3x3 a_rotate)
{
	m_rot = QuatFromAxisAngle(a_axis, a_radians); //glm::angleAxis(a_radians, a_axis);
}

float3 TransformSystem::Scale(const ECSEntity a_entity, const float3x3 a_scale)
{

}

void TransformSystem::SetPosition(const ECSEntity a_entity, const float3 a_position)
{

}

void TransformSystem::SetRotation(const ECSEntity a_entity, const float3x3 a_rotation)
{

}

void TransformSystem::SetScale(const ECSEntity a_entity, const float3x3 a_scale)
{

}
