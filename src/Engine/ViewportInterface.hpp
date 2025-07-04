#pragma once

#include <concepts>
#include "Storage/BBString.h"
#include "Slice.h"
#include "Enginefwd.hpp"

namespace BB
{
	struct ViewportRect
	{
		uint2 size;
		uint2 offset;
	};

	class Viewport
	{
	public:
		void Init(const uint2 a_extent, const int2 a_offset, const StackString<32>& a_name);

		bool PositionWithinViewport(const uint2 a_pos) const;
        bool ScreenToViewportMousePosition(const float2 a_pos, float2& a_new_pos) const;

		float4x4 CreateProjection(const float a_fov, const float a_near_field, const float a_far_field) const;

		void SetExtent(const uint2 a_extent);
		void SetOffset(const int2 a_offset);
		uint2 GetExtent() const;
		int2 GetOffset() const;

	private:
		StackString<32> m_name;
		uint2 m_extent;
		int2 m_offset;
	};
}
