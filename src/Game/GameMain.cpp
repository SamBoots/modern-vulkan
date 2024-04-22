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

void DungeonMap::Init()
{
	m_dungeon_map_memory = MemoryArenaCreate();
	m_before_mem_mark = MemoryArenaGetMemoryMarker(m_dungeon_map_memory);
}

void DungeonMap::Destroy()
{
	m_map.Destroy();
	MemoryArenaFree(m_dungeon_map_memory);
}

void DungeonMap::CreateMap(const uint32_t a_map_size_x, const uint32_t a_map_size_y)
{
	m_map_size_x = a_map_size_x;
	m_map_size_y = a_map_size_y;
	m_map.Init(m_dungeon_map_memory, m_map_size_x * m_map_size_y);
}

void DungeonMap::DestroyMap()
{
	m_map.Destroy();
}

bool DungeonGame::InitGame(MemoryArena& a_arena, const uint32_t a_scene_hierarchies)
{
	m_scene_hierarchies.Init(a_arena, a_scene_hierarchies);
	
	return true;
}

bool DungeonGame::Update(MemoryArena& a_temp_arena)
{
	(void)a_temp_arena;
	return true;
}

void DungeonGame::Destroy()
{

}
