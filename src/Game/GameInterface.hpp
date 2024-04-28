#pragma once

#include <concepts>
#include "MemoryArena.hpp"
#include "SceneHierarchy.hpp"
#include "HID.h"

namespace BB
{
	template <typename T>
	concept is_game_interface = requires(T v, int scene_count, MemoryArena & arena, const Slice<InputEvent> input_events)
	{
		{v.InitGame(arena, scene_count)} -> std::same_as<bool>;
		{v.Update(arena, input_events)} -> std::same_as<bool>;
		// maybe ifdef this for editor
		v.DisplayImGuiInfo();
		v.Destroy();

		{v.GetSceneHierarchies()} -> std::same_as<BB::StaticArray<BB::SceneHierarchy>&>;
	};
}
