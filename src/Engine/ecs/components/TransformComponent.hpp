#pragma once
#include "BBMemory.h"
#include "ecs/ECSBase.hpp"

namespace BB
{
	class TransformComponent
	{
	public:
		void Translate(const float3 a_translation);
		void Rotate(const float3 a_axis, const float a_radians);

		void SetRotation(const float3 a_axis, const float a_radians);

		const float4x4 CreateMatrix();

		float3 m_pos;
		Quat m_rot;
		float3 m_scale = float3(1.f);
	};

	using TransformComponentPool = ECSComponentBase<TransformComponent, TRANSFORM_ECS_SIGNATURE>;
	static_assert(is_ecs_component_map<TransformComponentPool, TransformComponent>);
}
