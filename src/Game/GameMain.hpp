#pragma once
#include "ViewportInterface.hpp"
#include "Camera.hpp"

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

		SceneObjectHandle CreateSceneObjectFloor(MemoryArena& a_temp_arena, SceneHierarchy& a_scene_hierarchy, const float3 a_pos);
		SceneObjectHandle CreateSceneObjectWalls(MemoryArena& a_temp_arena, SceneHierarchy& a_scene_hierarchy, const float3 a_pos);

		struct DungeonTile
		{
			bool walkable;
		};
		inline DungeonTile GetTile(const uint32_t a_x, const uint32_t a_y) const
		{
			return m_map[GetMapIndexFromXY(a_x, a_y)];
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
		uint2 m_spawn_point;
	};

	class Player
	{
	public:
		Player() = default;

		float3 Move(const float3 a_translation);
		void SetPosition(const float3 a_position);

		float4x4 CalculateView() const;


		float3 GetPosition() const
		{
			return m_position;
		}
	private:
		float3 m_position;
		float3 m_up{ 0.0f, 1.0f, 0.0f };
		float3 m_forward{ 0.0f, 0.0f, 1.0f };
	};

	class DungeonGame
	{
	public:
		bool Init(const uint2 a_game_viewport_size, const uint32_t a_back_buffer_count);
		bool Update(const float a_delta_time);
		bool HandleInput(const float a_delta_time, const Slice<InputEvent> a_input_events);
		// maybe ifdef this for editor
		void DisplayImGuiInfo();
		void Destroy();

		Viewport& GetViewport() { return m_viewport; }
		SceneHierarchy& GetSceneHierarchy() { return m_scene_hierarchy; }

	private:
		void ToggleFreeCam();
		MemoryArena m_game_memory;

		Player m_player;
		Viewport m_viewport;
		SceneHierarchy m_scene_hierarchy;
		DungeonMap m_dungeon_map;

		// debug
		struct FreeCameraOption
		{
			bool freeze_free_cam;
			bool use_free_cam;
			FreeCamera camera{};
			float speed = 0.25f;
			float min_speed = 0.1f;
			float max_speed = 1.0f;
		};
		FreeCameraOption m_free_cam;
	};
	static_assert(is_interactable_viewport_interface<DungeonGame>);
}
