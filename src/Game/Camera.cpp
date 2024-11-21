#include "Camera.hpp"
#include "Math.inl"

#include "Utils.h"

using namespace BB;

constexpr float3 UP_VECTOR { 0, 1.f, 0 };
constexpr float3 STANDARD_CAM_FRONT { 0, 0, -1 };

FreeCamera::FreeCamera()
{
	m_forward = STANDARD_CAM_FRONT;
	m_yaw = 90.f;
	m_pitch = 0;
}

void FreeCamera::Move(const float3 a_movement)
{
	float3 velocity{0, 0, 0};
	velocity = velocity + m_forward * static_cast<float>(a_movement.z);
	velocity = velocity + Float3Normalize(Float3Cross(m_forward, m_up)) * a_movement.x; //glm::normalize(glm::cross(m_forward, m_up)) * a_movement.x;
	velocity = velocity + UP_VECTOR * a_movement.y;

	velocity = velocity * m_speed;

	m_pos = m_pos + velocity;
}

void FreeCamera::Rotate(const float a_yaw, const float a_pitch)
{
	m_yaw += a_yaw * m_speed;
	m_pitch += a_pitch * m_speed;
	m_pitch = Clampf(m_pitch, -90.f, 90.f);

	float3 direction{};
	direction.x = cosf(m_yaw) * cosf(m_pitch);//cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
	direction.y = sinf(m_pitch);				//sin(glm::radians(m_Pitch));
	direction.z = sinf(m_yaw) * cosf(m_pitch);//sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));

	m_forward = Float3Normalize(direction); //glm::normalize(t_Direction);
	m_Right = Float3Normalize(Float3Cross(UP_VECTOR, m_forward));
}

void FreeCamera::SetSpeed(const float a_speed_mod)
{
	m_speed = a_speed_mod;
}

void FreeCamera::SetPosition(const float3 a_pos)
{
	m_pos = a_pos;
}

void FreeCamera::SetUp(const float3 a_up)
{
	m_up = a_up;
}

const float4x4 FreeCamera::CalculateView() const
{
	return Float4x4Lookat(m_pos, m_pos + m_forward, m_up);
}

const float3 FreeCamera::GetPosition() const
{
	return m_pos;
}
