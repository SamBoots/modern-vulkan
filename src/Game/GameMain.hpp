#pragma once
#include "GameInterface.hpp"

namespace BB
{
	class DungeonMap
	{
	public:
		void Init(const uint32_t a_map_size_x, const uint32_t a_map_size_y);
		void Destroy();
		void CreateMap();

		enum class DUNGEON_TILE
		{
			INACCESSABLE,
			WALKABLE
		};

		struct DungeonTile
		{
			bool walkable;

		};

	private:
		
		uint32_t m_map_size_x;
		uint32_t m_map_size_y;
 		StaticArray<DungeonTile> m_map;

		MemoryArena m_dungeon_map_memory;
	};



	class DungeonGame
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
