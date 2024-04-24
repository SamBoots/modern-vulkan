#pragma once
#include "GameInterface.hpp"

namespace BB
{
	enum class DUNGEON_TILE : uint32_t;

	class DungeonRoom
	{
	public:
		void CreateRoom(MemoryArena& a_arena, const char* a_image_path);

		uint32_t GetSizeX() const { return m_room_size_x; }
		uint32_t GetSizeY() const { return m_room_size_y; }
		inline DUNGEON_TILE GetTile(const uint32_t a_x, const uint32_t a_y) const
		{
			return m_room_tiles[GetRoomIndexFromXY(a_x, a_y)];
		}

	private:
		inline uint32_t GetRoomIndexFromXY(const uint32_t a_x, const uint32_t a_y) const
		{
			BB_ASSERT(a_x < m_room_size_x, "a_x is higher then m_room_size_x");
			BB_ASSERT(a_y < m_room_size_y, "a_y is higher then m_room_size_y");
			return a_x + a_y * m_room_size_x;
		}

		uint32_t m_room_size_x;
		uint32_t m_room_size_y;
		StaticArray<DUNGEON_TILE> m_room_tiles;
	};

	class DungeonMap
	{
	public:
		MemoryArenaMarker CreateMap(MemoryArena& a_game_memory, const uint32_t a_map_size_x, const uint32_t a_map_size_y, const Slice<DungeonRoom*> a_rooms);
		void DestroyMap();

		SceneObjectHandle CreateRenderObject(MemoryArena& a_temp_arena, SceneHierarchy& a_scene_hierarchy);

		struct DungeonTile
		{
			bool walkable;
		};
		inline DungeonTile GetTile(const uint32_t a_x, const uint32_t a_y) const
		{
			return m_map[GetMapIndexFromXY(a_x, a_y)];
		}

	private:
		inline uint32_t GetMapIndexFromXY(const uint32_t a_x, const uint32_t a_y) const
		{
			BB_ASSERT(a_x < m_map_size_x, "a_x is higher then m_map_size_x");
			BB_ASSERT(a_y < m_map_size_y, "a_y is higher then m_map_size_y");
			return a_x + a_y * m_map_size_x;
		}
		inline void GetMapXYFromIndex(const uint32_t a_index, uint32_t& a_x, uint32_t& a_y)
		{
			BB_ASSERT(m_map.size() < a_index, "a_index is higher then the size of the map");
			a_x = a_index % m_map_size_x;
			a_y = a_index / m_map_size_y;
		}

		uint32_t m_map_size_x;
		uint32_t m_map_size_y;
 		StaticArray<DungeonTile> m_map;
	};

	class DungeonGame
	{
	public:
		bool InitGame(MemoryArena& a_arena, const uint32_t a_scene_count);
		bool Update(MemoryArena& a_temp_arena, const Slice<InputEvent> a_input_events);
		void Destroy();

		StaticArray<SceneHierarchy>& GetSceneHierarchies() { return m_scene_hierarchies; }

	private:
		MemoryArena m_game_memory;

		StaticArray<SceneHierarchy> m_scene_hierarchies;
		StaticArray<DungeonRoom> m_dungeon_rooms;
		DungeonMap m_dungeon_map;
	};
}
