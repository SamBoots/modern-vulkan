#pragma once
#include "ECSBase.hpp"
#include "Math.inl"

#include "MemoryArena.hpp"

namespace BB
{
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

	class TransformSystem
	{
	public:
		bool Init(MemoryArena& a_arena, const uint32_t a_entity_max);

		bool EntityAddTransform(const ECSEntity a_entity, const float3 a_pos = float3(0.f), const float3x3 a_rotation = Float3x3Identity(), const float3 a_scale = float3(1.f));
		float3 Translate(const ECSEntity a_entity, const float3 a_translate);
		float3x3 Rotate(const ECSEntity a_entity, const float3x3 a_rotate);
		float3 Scale(const ECSEntity a_entity, const float3x3 a_scale);

		void SetPosition(const ECSEntity a_entity, const float3 a_position);
		void SetRotation(const ECSEntity a_entity, const float3x3 a_rotation);
		void SetScale(const ECSEntity a_entity, const float3x3 a_scale);

	private:
		StaticArray<bool> m_dirty_matrices;
		// all entities are registered here...
	};
}
