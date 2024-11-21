#pragma once
#include "Common.h"

namespace BB
{
	class FreeCamera
	{
	public:
		FreeCamera();

		void Move(const float3 a_movement);
		void Rotate(const float a_yaw, const float a_pitch);
		void SetSpeed(const float a_speed_mod);
		void SetPosition(const float3 a_pos);
		void SetUp(const float3 a_up);

		const float4x4 CalculateView() const;
		const float3 GetPosition() const;
	private:
		float m_yaw;
		float m_pitch;
		float m_speed;

		float3 m_pos;
		float3 m_forward;
		float3 m_Right;
		float3 m_up;
	};
}
