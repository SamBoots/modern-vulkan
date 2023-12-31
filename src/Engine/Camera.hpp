#pragma once
#include "Common.h"

namespace BB
{
	class Camera
	{
	public:
		Camera(const float3 a_Pos, const float a_CamSpeed = 0.15f);

		void Move(const float3 a_movement);
		void Rotate(const float a_Yaw, const float a_Pitch);
		void SetSpeed(const float a_SpeedModifier);

		const float4x4 CalculateView();
	private:
		float m_Yaw;
		float m_Pitch;
		float m_Speed;

		float3 m_pos;
		float3 m_forward;
		float3 m_Right;
		float3 m_up;
	};
}
