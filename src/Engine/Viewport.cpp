#include "ViewportInterface.hpp"
#include "Math/Math.inl"

using namespace BB;

void Viewport::Init(const uint2 a_extent, const int2 a_offset, const StackString<32>& a_name)
{
	m_name = a_name;
	m_extent = a_extent;
	m_offset = a_offset;
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

bool Viewport::ScreenToViewportMousePosition(const float2 a_pos, float2& a_new_pos) const
{
    if (!PositionWithinViewport(uint2(static_cast<uint32_t>(a_pos.x), static_cast<uint32_t>(a_pos.y))))
        return false;

    a_new_pos.x = a_pos.x - static_cast<float>(m_offset.x);
    a_new_pos.y = a_pos.y - static_cast<float>(m_offset.y);
    return true;
}

float4x4 Viewport::CreateProjection(const float a_fov, const float a_near_field, const float a_far_field) const
{
	return Float4x4Perspective(ToRadians(a_fov), static_cast<float>(m_extent.x) / static_cast<float>(m_extent.y), a_near_field, a_far_field);
}

void Viewport::SetExtent(const uint2 a_extent)
{
	m_extent = a_extent;
}

void Viewport::SetOffset(const int2 a_offset)
{
	m_offset = a_offset;
}

uint2 Viewport::GetExtent() const
{ 
	return m_extent; 
}
int2 Viewport::GetOffset() const
{ 
	return m_offset; 
}
