#include "FreeCamera.hpp"
#include "Math/Math.inl"

#include "Utils.h"

using namespace BB;

FreeCamera::FreeCamera()
{
    m_yaw = 90.f;
    m_pitch = 0;
}

void FreeCamera::Move(const float3 a_movement)
{
    const float3 forward = GetForward();
    const float3 right = GetRight();
    const float3 up = GetUp(right, forward);

    const float3 velocity = right * a_movement.x + up * a_movement.y + forward * a_movement.z;
    m_velocity = m_velocity + velocity * m_speed;
}

void FreeCamera::Rotate(const float a_yaw, const float a_pitch)
{
    m_yaw += a_yaw;
    m_pitch += a_pitch;
    m_pitch = Clampf(m_pitch, -90.f, 90.f);
}

void FreeCamera::AddSpeed(const float a_speed)
{
    float new_speed = m_speed + a_speed * ((m_speed + 2.2f) * 0.022f);
    if (new_speed > m_max_speed)
        new_speed = m_max_speed;
    else if (new_speed < m_min_speed)
        new_speed = m_min_speed;
    m_speed = new_speed;
}

void FreeCamera::SetPosition(const float3 a_pos)
{
    m_pos = a_pos;
}

void FreeCamera::SetVelocity(const float3 a_velocity)
{
    m_velocity = a_velocity;
}

float3 FreeCamera::GetRight() const
{
    return -Float3Normalize(float3(
        cosf(m_yaw - PI_F/2.f),
        0.f,
        sinf(m_yaw - PI_F/2.f)));
}

float3 FreeCamera::GetUp() const
{
    return Float3Cross(GetRight(), GetForward());
}

float3 FreeCamera::GetUp(const float3 a_right, const float3 a_forward) const
{
    return Float3Cross(a_right, a_forward);
}

float3 FreeCamera::GetForward() const
{
    return Float3Normalize(float3(
        cosf(m_yaw) * cosf(m_pitch),
        sinf(m_pitch),
        sinf(m_yaw) * cosf(m_pitch)));
}

void FreeCamera::SetYawPitchFromForward(const float3 a_forward)
{
    m_yaw = atan2f(a_forward.x, a_forward.z);
    m_pitch = asin(a_forward.y);
}

void FreeCamera::Update(const float a_delta_time)
{
    m_pos = m_pos + m_velocity * a_delta_time;
    m_velocity = m_velocity * (1.f - m_velocity_speed * a_delta_time);
}

const float4x4 FreeCamera::CalculateView() const
{
    return Float4x4Lookat(m_pos, m_pos + GetForward(), GetUp());
}

const float3 FreeCamera::GetPosition() const
{
    return m_pos;
}
