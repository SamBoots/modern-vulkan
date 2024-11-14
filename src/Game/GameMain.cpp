#include "GameMain.hpp"
#include "BBImage.hpp"
#include "MaterialSystem.hpp"

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
	m_map.fill(DungeonTile{});

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
	SceneObjectHandle map_obj{};
	MemoryArenaScope(a_temp_arena)
	{
		Vertex bot_left;
		bot_left.normal = float3(0.f, 1.f, 0.1f);
		bot_left.uv = float2(0.f, 0.f);
		bot_left.color = float4(1.f, 1.f, 1.f, 1.f);

		Vertex bot_right;
		bot_right.normal = float3(0.f, 1.f, 0.1f);
		bot_right.uv = float2(1.f, 0.f);
		bot_right.color = float4(1.f, 1.f, 1.f, 1.f);

		Vertex top_left;
		top_left.normal = float3(0.f, 1.f, 0.1f);
		top_left.uv = float2(0.f, 1.f);
		top_left.color = float4(1.f, 1.f, 1.f, 1.f);

		Vertex top_right;
		top_right.normal = float3(0.f, 1.f, 0.1f);
		top_right.uv = float2(1.f, 1.f);
		top_right.color = float4(1.f, 1.f, 1.f, 1.f);

		StaticArray<Vertex> vertices;
		vertices.Init(a_temp_arena, m_map.size() * 4);
		StaticArray<uint32_t> indices;
		indices.Init(a_temp_arena, m_map.size() * 6);
		// optimize this
		for (uint32_t y = 0; y < m_map_size_y; y++)
		{
			for (uint32_t x = 0; x < m_map_size_x; x++)
			{
				const DungeonTile& tile = GetTile(x, y);
				if (tile.walkable)
				{
					const float3 pos_bot_left = float3(static_cast<float>(x), static_cast<float>(y), 0.f);
					const float3 pos_bot_right = float3(static_cast<float>(x + 1), static_cast<float>(y), 0.f);
					const float3 pos_top_left = float3(static_cast<float>(x), static_cast<float>(y + 1), 0.f);
					const float3 pos_top_right = float3(static_cast<float>(x + 1), static_cast<float>(y + 1), 0.f);

					bot_left.position = pos_bot_left;
					bot_right.position = pos_bot_right;
					top_left.position = pos_top_left;
					top_right.position = pos_top_right;

					const uint32_t current_index = indices.size();
					const uint32_t quad_indices[] = {
						current_index,
						current_index + 1,
						current_index + 2,
						current_index + 2,
						current_index + 3,
						current_index + 0
					};

					vertices.push_back(bot_left);
					vertices.push_back(bot_right);
					vertices.push_back(top_left);
					vertices.push_back(top_right);
					indices.push_back(quad_indices, _countof(quad_indices));
				}
			}
		}
		CreateMeshInfo create_mesh_info;
		create_mesh_info.vertices = Slice(vertices.data(), vertices.size());
		create_mesh_info.indices = Slice(indices.data(), indices.size());
		Mesh mesh = CreateMesh(create_mesh_info);
		MeshMetallic material_info;
		material_info.metallic_factor = 1.0f;
		material_info.roughness_factor = 0.0f;
		material_info.base_color_factor = float4(1.f);
		material_info.albedo_texture = GetDebugTexture();
		material_info.normal_texture = GetWhiteTexture();

		SceneMeshCreateInfo mesh_info;
		mesh_info.mesh = mesh;
		mesh_info.index_start = 0;
		mesh_info.index_count = indices.size();
		mesh_info.master_material = Material::GetDefaultMasterMaterial(PASS_TYPE::SCENE, MATERIAL_TYPE::MATERIAL_3D);
		mesh_info.material_data = material_info;
		map_obj = a_scene_hierarchy.CreateSceneObjectMesh(float3(3.f, 0.f, 0.f), mesh_info, "dungeon map");
	}
	return map_obj;
}

bool DungeonGame::InitGame()
{
	m_game_memory = MemoryArenaCreate();
	const uint32_t back_buffer_count = GetRenderIO().frame_count;
	m_scene_hierarchy.Init(m_game_memory, back_buffer_count, Asset::FindOrCreateString("game hierarchy"));
	m_scene_hierarchy.SetClearColor(float3(0.3f, 0.3f, 0.3f));
	DungeonRoom room;
	room.CreateRoom(m_game_memory, "../../resources/game/dungeon_rooms/map1.bmp");
	DungeonRoom* roomptr = &room;
	m_dungeon_map.CreateMap(m_game_memory, 30, 30, Slice(&roomptr, 1));
	MemoryArenaScope(m_game_memory)
	{
		m_dungeon_map.CreateRenderObject(m_game_memory, m_scene_hierarchy);
	}
	return true;
}

bool DungeonGame::Update(const Slice<InputEvent> a_input_events, const float4x4* a_overwrite_view_matrix, const float3* a_overwrite_view_pos)
{
	for (size_t i = 0; i < a_input_events.size(); i++)
	{
		const InputEvent& ip = a_input_events[i];
		(void)ip;
	}

	if (a_overwrite_view_matrix && a_overwrite_view_pos)
		m_scene_hierarchy.SetView(*a_overwrite_view_matrix, *a_overwrite_view_pos);
	else
	{
		// normal cam stuff
	}

	return true;
}

// maybe ifdef this for editor
void DungeonGame::DisplayImGuiInfo()
{

}

void DungeonGame::Destroy()
{
	MemoryArenaFree(m_game_memory);
}
