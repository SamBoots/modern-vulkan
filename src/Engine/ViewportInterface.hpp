#pragma once

#include <concepts>
#include "SceneHierarchy.hpp"
#include "HID.h"

namespace BB
{
	struct ViewportRect
	{
		uint2 size;
		uint2 offset;
	};

	template <typename T>
	concept is_viewport_interface = requires(
		T v,
		const float3 * a_overwrite_view_pos,
		float4x4 * a_overwrite_view_matrix,
		const Slice<InputEvent> a_input_events,
		const uint2 a_point)
	{
		{ v.Init() } -> std::same_as<bool>;
		{ v.Update(a_input_events, a_overwrite_view_matrix, a_overwrite_view_pos) } -> std::same_as<bool>;
		// maybe ifdef this for editor
		v.DisplayImGuiInfo();
		v.Destroy();

		{ v.IsPointInViewport(a_point) } -> std::same_as<bool>;
		{ v.GetViewportRect() } -> std::same_as<BB::ViewportRect>;
		{ v.GetSceneHierarchy() } -> std::same_as<BB::SceneHierarchy&>;
	};
}
