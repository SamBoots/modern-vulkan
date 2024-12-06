#pragma once
#include "BBMemory.h"
#include "ECSBase.hpp"

namespace BB
{
	class TransformComponent
	{
	public:
		TransformComponent(const float3 a_position = float3(0.f, 0.f, 0.f));
		TransformComponent(const float3 a_position, const float3 a_axis, const float a_radians);
		TransformComponent(const float3 a_position, const Quat a_rotation, const float3 a_scale);
		TransformComponent(const float3 a_position, const float3 a_axis, const float a_radians, const float3 a_scale);

		void Translate(const float3 a_translation);
		void Rotate(const float3 a_axis, const float a_radians);

		void SetPosition(const float3 a_position);
		void SetRotation(const float3 a_axis, const float a_radians);
		void SetScale(const float3 a_scale);

		const float4x4 CreateMatrix();

		float3 m_pos;
		Quat m_rot;
		float3 m_scale;
	};

	class TransformComponentPool
	{
	public:
		void Init(struct MemoryArena& a_arena, const uint32_t a_transform_count);

		bool CreateComponent(const ECSEntity a_entity);
		bool CreateComponent(const ECSEntity a_entity, const TransformComponent& a_component);
		bool FreeComponent(const ECSEntity a_entity);
		TransformComponent& GetComponent(const ECSEntity a_entity);

		inline ECSSignatureIndex GetSignatureIndex() const
		{
			return TRANSFORM_ECS_SIGNATURE;
		}
			
	private:
		bool EntityInvalid(const ECSEntity a_entity) const;

		// components equal to entities.
		StaticArray<TransformComponent> m_components;
	};
	static_assert(is_ecs_component_map<TransformComponentPool, TransformComponent>);
}
