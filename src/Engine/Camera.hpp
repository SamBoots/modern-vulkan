#pragma once
#include "Common.h"

namespace BB
{
	class Camera
	{
	public:
		Camera(const float3 a_pos, const float a_cam_speed = 0.15f);

		void Move(const float3 a_movement);
		void Rotate(const float a_yaw, const float a_pitch);
		void SetSpeed(const float a_speed_mod);

		const float4x4 CalculateView() const;
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
