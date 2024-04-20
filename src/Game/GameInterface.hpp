#pragma once

#include <concepts>
#include "MemoryArena.hpp"
#include "SceneHierarchy.hpp"

namespace BB
{
	template <typename T>
	concept is_game_interface = requires(T v, int scene_count, MemoryArena& arena)
	{
		{v.InitGame(arena, scene_count)} -> std::same_as<bool>;
		{v.Update(arena)} -> std::same_as<bool>;
		{v.Destroy()};

		{ v.GetSceneHierarchies() } -> std::same_as<BB::StaticArray<BB::SceneHierarchy>&>;
	};

	class DefaultGame
	{
	public:
		bool InitGame(MemoryArena& a_arena, const uint32_t a_scene_count);
		bool Update(MemoryArena& a_temp_arena);
		void Destroy();

		StaticArray<SceneHierarchy>& GetSceneHierarchies() { return m_scene_hierarchies; }

	private:
		StaticArray<SceneHierarchy> m_scene_hierarchies;
	};
}
