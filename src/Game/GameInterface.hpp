#pragma once

#include <concepts>
#include "SceneHierarchy.hpp"
#include "HID.h"

namespace BB
{
	template <typename T>
	concept is_game_interface = requires(
		T v,
		const float3 * overwrite_view_pos,
		float4x4* overwrite_view_matrix, 
		const Slice<InputEvent> input_events)
	{
		{v.InitGame()} -> std::same_as<bool>;
		{v.Update(input_events, overwrite_view_matrix, overwrite_view_pos)} -> std::same_as<bool>;
		// maybe ifdef this for editor
		v.DisplayImGuiInfo();
		v.Destroy();

		{v.GetSceneHierarchy()} -> std::same_as<BB::SceneHierarchy&>;
	};
}
