#pragma once

#include <concepts>
#include "Storage/BBString.h"
#include "Slice.h"

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
		void Init(const uint2 a_extent, const int2 a_offset);

		bool PositionWithinViewport(const uint2 a_pos) const;

		float4x4 CreateProjection(const float a_fov, const float a_near_field, const float a_far_field) const;

		void SetExtent(const uint2 a_extent);
		uint2 GetExtent() const;
		int2 GetOffset() const;

	private:
		uint2 m_extent;
		int2 m_offset;
	};

	template <typename T>
	concept is_interactable_viewport_interface = requires(T v, const float a_delta_time, Slice<struct InputEvent> a_input_events)
	{
		{ v.Update(a_delta_time) } -> std::same_as<bool>;
		{ v.HandleInput(a_delta_time, a_input_events) } -> std::same_as<bool>;

		{ v.GetViewport() } -> std::same_as<BB::Viewport&>;
	};
}
