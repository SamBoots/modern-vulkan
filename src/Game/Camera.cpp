#include "Camera.hpp"
#include "Math/Math.inl"

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
	velocity = velocity + Float3Normalize(Float3Cross(m_forward, m_up)) * a_movement.x;
	velocity = velocity + UP_VECTOR * a_movement.y;

	m_velocity = m_velocity + velocity * m_speed;
}

void FreeCamera::Rotate(const float a_yaw, const float a_pitch)
{
	m_yaw += a_yaw;
	m_pitch += a_pitch;
	m_pitch = Clampf(m_pitch, -90.f, 90.f);

	float3 direction{};
	direction.x = cosf(m_yaw) * cosf(m_pitch);
	direction.y = sinf(m_pitch);
	direction.z = sinf(m_yaw) * cosf(m_pitch);

	m_forward = Float3Normalize(direction);
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

void FreeCamera::SetVelocity(const float3 a_velocity)
{
	m_velocity = a_velocity;
}

void FreeCamera::SetUp(const float3 a_up)
{
	m_up = a_up;
}

void FreeCamera::Update(const float a_delta_time)
{
	const float3 velocity = m_velocity * m_velocity_speed * a_delta_time;
	m_velocity = m_velocity - velocity;
	m_pos = m_pos + velocity;
}

const float4x4 FreeCamera::CalculateView() const
{
	return Float4x4Lookat(m_pos, m_pos + m_forward, m_up);
}

const float3 FreeCamera::GetPosition() const
{
	return m_pos;
}
