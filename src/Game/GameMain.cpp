#include "GameMain.hpp"
#include "BBImage.hpp"

using namespace BB;

enum class BB::DUNGEON_TILE : uint32_t
{
	INACCESSABLE = 0,
	WALKABLE = 1,
	ENUM_SIZE = 2
};

constexpr Color dungeon_room_tile_colors[static_cast<uint32_t>(DUNGEON_TILE::ENUM_SIZE)]
{
	Color{0, 0, 0, 255},		// INACCESSABLE
	Color{255, 255, 255, 255 }	// WALKABLE
};

void DungeonRoom::CreateRoom(MemoryArena& a_arena, const char* a_image_path)
{
	BBImage image{};
	MemoryArenaScope(a_arena)
	{
		image.Init(a_arena, a_image_path);
		m_room_size_x = image.GetWidth();
		m_room_size_y = image.GetHeight();
	}

	// we share the same memory space......
	m_room_tiles.Init(a_arena, m_room_size_x * m_room_size_y);

	image = {};
	MemoryArenaScope(a_arena)
	{
		image.Init(a_arena, a_image_path, 4);
		const Color* pixels =  reinterpret_cast<const Color*>(image.GetPixels());

		for (size_t i = 0; i < m_room_tiles.capacity(); i++)
		{
			if (*pixels == dungeon_room_tile_colors[static_cast<uint32_t>(DUNGEON_TILE::INACCESSABLE)])
			{
				m_room_tiles.emplace_back(DUNGEON_TILE::INACCESSABLE);
			}
			else if (*pixels == dungeon_room_tile_colors[static_cast<uint32_t>(DUNGEON_TILE::WALKABLE)])
			{
				m_room_tiles.emplace_back(DUNGEON_TILE::WALKABLE);
			}
			else
			{
				BB_ASSERT(false, "invalid color found in the map image");
			}
			++pixels;
		}
	}

	BB_ASSERT(m_room_tiles.size() == m_room_tiles.capacity(), "room tiles are not all filled in");
}

MemoryArenaMarker DungeonMap::CreateMap(MemoryArena& a_game_memory, const uint32_t a_map_size_x, const uint32_t a_map_size_y, const Slice<DungeonRoom*> a_rooms)
{
	m_map_size_x = a_map_size_x;
	m_map_size_y = a_map_size_y;
	m_map.Init(a_game_memory, m_map_size_x * m_map_size_y);

	for (size_t i = 0; i < a_rooms.size(); i++)
	{
		const DungeonRoom& room = *a_rooms[i];
		const uint32_t max_x = m_map_size_x - room.GetSizeX();
		const uint32_t max_y = m_map_size_y - room.GetSizeY();

		const uint32_t start_pos_x = Random::Random(1, max_x);
		const uint32_t start_pos_y = Random::Random(1, max_y);

		const uint32_t end_pos_x = start_pos_x + room.GetSizeX();
		const uint32_t end_pos_y = start_pos_y + room.GetSizeY();

		// try to load the map reverse so you don't need to track these two variables
		uint32_t room_x = 0;
		uint32_t room_y = 0;
		for (uint32_t y = start_pos_y; y < end_pos_y; y++)
		{
			for (uint32_t x = start_pos_x; x < end_pos_x; x++)
			{
				const uint32_t index = GetMapIndexFromXY(x, y);

				switch (room.GetTile(room_x++, room_y))
				{
				case DUNGEON_TILE::INACCESSABLE:
					m_map[index].walkable = false;
					break;
				case DUNGEON_TILE::WALKABLE:
					m_map[index].walkable = true;
					break;
				default:
					BB_ASSERT(false, "should not hit this DUNGEON_TILE enum");
					break;
				}
			}
			room_x = 0;
			++room_y;
		}
	}

	return MemoryArenaGetMemoryMarker(a_game_memory);
}

void DungeonMap::DestroyMap()
{
	m_map.Destroy();
	m_map_size_x = 0;
	m_map_size_y = 0;
}

SceneObjectHandle DungeonMap::CreateRenderObject(MemoryArena& a_temp_arena, SceneHierarchy& a_scene_hierarchy)
{
	const SceneObjectHandle map_obj = a_scene_hierarchy.CreateSceneObject(float3(0, 0, 0), "dungeon map");

	MemoryArenaScope(a_temp_arena)
	{
		StaticArray<Vertex> vertices;
		StaticArray<uint32_t> indices;

		MeshHandle mesh;
		a_scene_hierarchy.SetMesh(map_obj, mesh);
	}

	return map_obj;
}

bool DungeonGame::InitGame(MemoryArena& a_arena, const uint32_t a_scene_hierarchies)
{
	m_scene_hierarchies.Init(a_arena, a_scene_hierarchies);
	
	return true;
}

bool DungeonGame::Update(MemoryArena& a_temp_arena, const Slice<InputEvent> a_input_events)
{
	(void)a_temp_arena;

	for (size_t i = 0; i < a_input_events.size(); i++)
	{
		const InputEvent& ip = a_input_events[i];
		(void)ip;
	}

	return true;
}

void DungeonGame::Destroy()
{
	MemoryArenaFree(m_game_memory);
}
