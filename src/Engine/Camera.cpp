#include "Camera.hpp"
#include "Math.inl"

#include "Utils.h"

using namespace BB;

constexpr float3 UP_VECTOR { 0, 1.f, 0 };
constexpr float3 STANDARD_CAM_FRONT { 0, 0, -1 };

Camera::Camera(const float3 a_Pos, const float a_CamSpeed)
{
	m_pos = a_Pos;
	m_Speed = a_CamSpeed;

	float3 t_Direction = Float3Normalize(m_pos);

	m_Right = Float3Normalize(Float3Cross(UP_VECTOR, t_Direction));
	m_up = Float3Cross(t_Direction, m_Right);
	m_forward = STANDARD_CAM_FRONT;

	m_Yaw = 90.f;
	m_Pitch = 0;
}

void Camera::Move(const float3 a_movement)
{
	float3 velocity{0, 0, 0};
	velocity = velocity + m_forward * static_cast<float>(a_movement.z);
	velocity = velocity + Float3Normalize(Float3Cross(m_forward, m_up)) * a_movement.x; //glm::normalize(glm::cross(m_forward, m_up)) * a_movement.x;
	velocity = velocity + UP_VECTOR * a_movement.y;

	velocity = velocity * m_Speed;

	m_pos = m_pos + velocity;
}

void Camera::Rotate(const float a_Yaw, const float a_Pitch)
{
	m_Yaw += a_Yaw * m_Speed;
	m_Pitch += a_Pitch * m_Speed;
	m_Pitch = Clampf(m_Pitch, -90.f, 90.f);

	float3 t_Direction{};
	t_Direction.x = cosf(m_Yaw) * cosf(m_Pitch);//cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
	t_Direction.y = sinf(m_Pitch);				//sin(glm::radians(m_Pitch));
	t_Direction.z = sinf(m_Yaw) * cosf(m_Pitch);//sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));

	m_forward = Float3Normalize(t_Direction); //glm::normalize(t_Direction);
}

void Camera::SetSpeed(const float a_SpeedModifier)
{
	m_Speed = a_SpeedModifier;
}

const float4x4 Camera::CalculateView()
{
	return Float4x4Lookat(m_pos, m_pos + m_forward, m_up);
}
