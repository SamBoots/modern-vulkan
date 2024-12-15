#include "TransformComponent.hpp"
#include "Math.inl"

#include "MemoryArena.hpp"

using namespace BB;

void TransformComponent::Translate(const float3 a_translation)
{
	m_pos = m_pos + a_translation;
}

void TransformComponent::Rotate(const float3 a_axis, const float a_radians)
{
	m_rot = m_rot * Quat{ a_axis.x,a_axis.y,a_axis.z, a_radians }; //glm::rotate(m_rot, a_radians, a_axis);
}

void TransformComponent::SetRotation(const float3 a_axis, const float a_radians)
{
	m_rot = QuatFromAxisAngle(a_axis, a_radians); //glm::angleAxis(a_radians, a_axis);
}

const float4x4 TransformComponent::CreateMatrix()
{
	float4x4 matrix = Float4x4FromTranslation(m_pos) * Float4x4FromQuat(m_rot);
	matrix = Float4x4Scale(matrix, m_scale);
	return matrix;
}
