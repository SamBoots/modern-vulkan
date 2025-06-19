#pragma once
#include "ViewportInterface.hpp"
#include "Storage/Array.h"
#include "Enginefwd.hpp"
#include "SceneHierarchy.hpp"
#include "lua/LuaEngine.hpp"

namespace BB
{
	enum class DUNGEON_TILE : uint32_t;

	class DungeonRoom
	{
	public:
		void CreateRoom(MemoryArena& a_arena, const char* a_image_path);

		int GetSizeX() const { return m_room_size_x; }
		int GetSizeY() const { return m_room_size_y; }
		inline DUNGEON_TILE GetTile(const int a_x, const int a_y) const
		{
			return m_room_tiles[GetRoomIndexFromXY(a_x, a_y)];
		}

	private:
		inline size_t GetRoomIndexFromXY(const int a_x, const int a_y) const
		{
			BB_ASSERT(a_x < m_room_size_x, "a_x is higher then m_room_size_x");
			BB_ASSERT(a_y < m_room_size_y, "a_y is higher then m_room_size_y");
			return static_cast<size_t>(a_x + a_y * m_room_size_x);
		}

		int m_room_size_x;
		int m_room_size_y;
		StaticArray<DUNGEON_TILE> m_room_tiles;
	};

	class DungeonMap
	{
	public:
		MemoryArenaMarker CreateMap(MemoryArena& a_game_memory, const int a_map_size_x, const int a_map_size_y, const Slice<DungeonRoom*> a_rooms);
		void DestroyMap();

		ECSEntity CreateEntityFloor(MemoryArena& a_temp_arena, SceneHierarchy& a_scene_hierarchy, const ECSEntity a_parent);
		ECSEntity CreateEntityWalls(MemoryArena& a_temp_arena, SceneHierarchy& a_scene_hierarchy, const ECSEntity a_parent);

		struct DungeonTile
		{
			bool walkable;
		};
		inline DungeonTile GetTile(const int a_x, const int a_y) const
		{
			return m_map[static_cast<size_t>(GetMapIndexFromXY(a_x, a_y))];
		}
		inline bool IsTileWalkable(const int a_x, const int a_y)
		{
			if (a_x >= m_map_size_x || a_y >= m_map_size_y || 0 > a_x || 0 > a_y)
				return false;

			return GetTile(a_x, a_y).walkable;
		}

		float3 GetSpawnPoint() const 
		{ 
			float3 spawn;
			spawn.x = static_cast<float>(m_spawn_point.x);
			spawn.y = 0;
			spawn.z = static_cast<float>(m_spawn_point.y);
			return spawn;
		}

	private:
		inline int GetMapIndexFromXY(const int a_x, const int a_y) const
		{
			BB_ASSERT(a_x <= m_map_size_x, "a_x is higher then m_map_size_x");
			BB_ASSERT(a_y <= m_map_size_y, "a_y is higher then m_map_size_y");
			return a_x + a_y * m_map_size_x;
		}
		inline void GetMapXYFromIndex(const int a_index, int& a_x, int& a_y)
		{
			BB_ASSERT(static_cast<int>(m_map.size()) < a_index, "a_index is higher then the size of the map");
			a_x = a_index % m_map_size_x;
			a_y = a_index / m_map_size_y;
		}

		int m_map_size_x;
		int m_map_size_y;
 		StaticArray<DungeonTile> m_map;
		int2 m_spawn_point;
	};

	class DungeonGame
	{
	public:
		bool Init(const uint2 a_game_viewport_size, const uint32_t a_back_buffer_count);
		bool Update(const float a_delta_time, const bool a_selected = true);
		// maybe ifdef this for editor
		void DisplayImGuiInfo();
		void Destroy();

        float3 GetCameraPos();
        float4x4 GetCameraView();
		Viewport& GetViewport() { return m_viewport; }
        InputChannelHandle GetInputChannel() const { return m_input_channel; }
        SceneHierarchy& GetSceneHierarchy() { return m_scene_hierarchy; }

	private:
		MemoryArena m_game_memory;

		Viewport m_viewport;
		SceneHierarchy m_scene_hierarchy;
		DungeonMap m_dungeon_map;
		ECSEntity m_dungeon_obj;
        LuaECSEngine m_context;
        InputChannelHandle m_input_channel;
	};
	static_assert(is_interactable_viewport_interface<DungeonGame>);
}
