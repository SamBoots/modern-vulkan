#pragma once

#include <concepts>
#include "MemoryArena.hpp"
#include "SceneHierarchy.hpp"

namespace BB
{
	template <typename T>
	concept is_game_interface = requires(T v, int scene_count, MemoryArena & arena)
	{
		{v.InitGame(arena, scene_count)} -> std::same_as<bool>;
		{v.Update(arena)} -> std::same_as<bool>;
		v.Destroy();

		{v.GetSceneHierarchies()} -> std::same_as<BB::StaticArray<BB::SceneHierarchy>&>;
	};
}
