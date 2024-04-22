#pragma once
#include "GameInterface.hpp"

namespace BB
{
	enum class DUNGEON_TILE : uint32_t;

	class DungeonRoom
	{
	public:
		void CreateRoom(MemoryArena& a_arena, const char* a_image_path);

		inline size_t GetRoomIndexFromXY(const size_t a_x, const size_t a_y) const
		{
			BB_ASSERT(a_x < m_room_size_x, "a_x is higher then m_room_size_x");
			BB_ASSERT(a_y < m_room_size_y, "a_y is higher then m_room_size_y");
			return a_x + a_y * m_room_size_x;
		}

	private:
		uint32_t m_room_size_x;
		uint32_t m_room_size_y;
		StaticArray<DUNGEON_TILE> m_room_tiles;
	};

	class DungeonMap
	{
	public:
		void Init();
		void Destroy();
		void CreateMap(const uint32_t a_map_size_x, const uint32_t a_map_size_y);
		void DestroyMap();

		struct DungeonTile
		{
			bool walkable;
		};

		inline size_t GetMapIndexFromXY(const size_t a_x, const size_t a_y) const
		{
			return a_x + a_y * m_map_size_x;
		}


	private:
		uint32_t m_map_size_x;
		uint32_t m_map_size_y;
 		StaticArray<DungeonTile> m_map;

		MemoryArena m_dungeon_map_memory;
		MemoryArenaMarker m_before_mem_mark;
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
