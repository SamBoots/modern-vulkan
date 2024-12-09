#include "TransformComponent.hpp"
#include "Math.inl"

#include "MemoryArena.hpp"

using namespace BB;

TransformComponent::TransformComponent(const float3 a_position)
	: TransformComponent(a_position, float3{0,0,0}, 0, float3{1,1,1}) {}

TransformComponent::TransformComponent(const float3 a_position, const float3 a_axis, const float a_radians)
	: TransformComponent(a_position, a_axis, a_radians, float3{1,1,1}) {}

TransformComponent::TransformComponent(const float3 a_position, const Quat a_rotation, const float3 a_scale)
	: m_pos(a_position), m_rot(a_rotation), m_scale(a_scale){};

TransformComponent::TransformComponent(const float3 a_position, const float3 a_axis, const float a_radians, const float3 a_scale)
	: m_pos(a_position), m_scale(a_scale)
{
	m_rot = QuatFromAxisAngle(a_axis, a_radians); //glm::angleAxis(glm::radians(a_radians), a_axis);
}

void TransformComponent::Translate(const float3 a_translation)
{
	m_pos = m_pos + a_translation;
}

void TransformComponent::Rotate(const float3 a_axis, const float a_radians)
{
	m_rot = m_rot * Quat{ a_axis.x,a_axis.y,a_axis.z, a_radians }; //glm::rotate(m_rot, a_radians, a_axis);
}

void TransformComponent::SetPosition(const float3 a_position)
{
	m_pos = a_position;
}

void TransformComponent::SetRotation(const float3 a_axis, const float a_radians)
{
	m_rot = QuatFromAxisAngle(a_axis, a_radians); //glm::angleAxis(a_radians, a_axis);
}

void TransformComponent::SetScale(const float3 a_scale)
{
	m_scale = a_scale;
}

const float4x4 TransformComponent::CreateMatrix()
{
	float4x4 matrix = Float4x4FromTranslation(m_pos) * Float4x4FromQuat(m_rot);
	matrix = Float4x4Scale(matrix, m_scale);
	return matrix;
}

void TransformComponentPool::Init(struct MemoryArena& a_arena, const uint32_t a_transform_count)
{
	m_components.Init(a_arena, a_transform_count);
	m_components.resize(a_transform_count);
}

bool TransformComponentPool::CreateComponent(const ECSEntity a_entity)
{
	if (EntityInvalid(a_entity))
		return false;

	new (&m_components[a_entity.index]) TransformComponent(float3(0.f, 0.f, 0.f));
	return true;
}

bool TransformComponentPool::CreateComponent(const ECSEntity a_entity, const TransformComponent& a_component)
{
	if (EntityInvalid(a_entity))
		return false;

	new (&m_components[a_entity.index]) TransformComponent(a_component);
	return true;
}

bool TransformComponentPool::FreeComponent(const ECSEntity a_entity)
{
	if (EntityInvalid(a_entity))
		return false;

	return true;
}

TransformComponent& TransformComponentPool::GetComponent(const ECSEntity a_entity) const
{
	BB_ASSERT(!EntityInvalid(a_entity), "entity entry is not valid!");
	return m_components[a_entity.index];
}

bool TransformComponentPool::EntityInvalid(const ECSEntity a_entity) const
{
	if (a_entity.index >= m_components.size())
		return true;
	return false;
}
