#include "ViewportInterface.hpp"
#include "Math.inl"

using namespace BB;

void Viewport::Init(const uint2 a_extent, const int2 a_offset, const StackString<32> a_name)
{
	m_extent = a_extent;
	m_offset = a_offset;
	m_name = a_name;
}

bool Viewport::PositionWithinViewport(const uint2 a_pos) const
{
	if (m_offset.x < static_cast<int>(a_pos.x) &&
		m_offset.y < static_cast<int>(a_pos.y) &&
		m_offset.x + static_cast<int>(m_extent.x) > static_cast<int>(a_pos.x) &&
		m_offset.y + static_cast<int>(m_extent.y) > static_cast<int>(a_pos.y))
		return true;
	return false;
}

float4x4 Viewport::CreateProjection(const float a_fov, const float a_near_field, const float a_far_field) const
{
	return Float4x4Perspective(ToRadians(a_fov), static_cast<float>(m_extent.x) / static_cast<float>(m_extent.y), a_near_field, a_far_field);
}
