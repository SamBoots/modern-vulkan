#include "GameMain.hpp"
#include "BBImage.hpp"
#include "MaterialSystem.hpp"
#include "imgui.h"

#include "Math.inl"

using namespace BB;

using QuadVertices = FixedArray<Vertex, 4>;

enum class BB::DUNGEON_TILE : uint32_t
{
	INACCESSABLE = 0,
	WALKABLE = 1,
	SPAWN_POINT = 2,
	ENUM_SIZE = 3
};

constexpr Color dungeon_room_tile_colors[static_cast<uint32_t>(DUNGEON_TILE::ENUM_SIZE)]
{
	Color{ 0, 0, 0, 255 },		    // INACCESSABLE
	Color{ 255, 255, 255, 255 },    // WALKABLE
	Color{ 255, 0, 0, 255 }	        // SPAWN_POINT
};

void DungeonRoom::CreateRoom(MemoryArena& a_arena, const char* a_image_path)
{
	BBImage image{};
	MemoryArenaScope(a_arena)
	{
		image.Init(a_arena, a_image_path);
		m_room_size_x = static_cast<int>(image.GetWidth());
		m_room_size_y = static_cast<int>(image.GetHeight());
	}

	// we share the same memory space......
	m_room_tiles.Init(a_arena, static_cast<uint32_t>(m_room_size_x * m_room_size_y));

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
			else if (*pixels == dungeon_room_tile_colors[static_cast<uint32_t>(DUNGEON_TILE::SPAWN_POINT)])
			{
				m_room_tiles.emplace_back(DUNGEON_TILE::SPAWN_POINT);
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

MemoryArenaMarker DungeonMap::CreateMap(MemoryArena& a_game_memory, const int a_map_size_x, const int a_map_size_y, const Slice<DungeonRoom*> a_rooms)
{
	m_map_size_x = a_map_size_x;
	m_map_size_y = a_map_size_y;
	m_map.Init(a_game_memory, static_cast<uint32_t>(m_map_size_x * m_map_size_y));
	m_map.fill(DungeonTile{});

	for (size_t i = 0; i < a_rooms.size(); i++)
	{
		const DungeonRoom& room = *a_rooms[i];
		const int max_x = m_map_size_x - room.GetSizeX();
		const int max_y = m_map_size_y - room.GetSizeY();

		const int start_pos_x = static_cast<int>(Random::Random(1, static_cast<uint32_t>(max_x)));
		const int start_pos_y = static_cast<int>(Random::Random(1, static_cast<uint32_t>(max_y)));

		const int end_pos_x = start_pos_x + room.GetSizeX();
		const int end_pos_y = start_pos_y + room.GetSizeY();

		// try to load the map reverse so you don't need to track these two variables
		int room_x = 0;
		int room_y = 0;
		for (int y = start_pos_y; y < end_pos_y; y++)
		{
			for (int x = start_pos_x; x < end_pos_x; x++)
			{
				const size_t index = GetMapIndexFromXY(x, y);

				switch (room.GetTile(room_x++, room_y))
				{
				case DUNGEON_TILE::INACCESSABLE:
					m_map[index].walkable = false;
					break;
				case DUNGEON_TILE::WALKABLE:
					m_map[index].walkable = true;
					break;
				case DUNGEON_TILE::SPAWN_POINT:
					m_map[index].walkable = true;
					m_spawn_point = { x, y };
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

SceneObjectHandle DungeonMap::CreateSceneObjectFloor(MemoryArena& a_temp_arena, SceneHierarchy& a_scene_hierarchy, const float3 a_pos)
{
	const float3 floor_pos = a_pos + float3(0.f, -1.f, 0.f);

	SceneObjectHandle map_obj{};
	MemoryArenaScope(a_temp_arena)
	{
		QuadVertices quad_vertices;
		quad_vertices[0].normal = float3(0.f, 1.f, 0.1f);
		quad_vertices[0].uv = float2(0.f, 1.f);
		quad_vertices[0].color = float4(1.f, 1.f, 1.f, 1.f);

		quad_vertices[1].normal = float3(0.f, 1.f, 0.1f);
		quad_vertices[1].uv = float2(1.f, 1.f);
		quad_vertices[1].color = float4(1.f, 1.f, 1.f, 1.f);

		quad_vertices[2].normal = float3(0.f, 1.f, 0.1f);
		quad_vertices[2].uv = float2(1.f, 0.f);
		quad_vertices[2].color = float4(1.f, 1.f, 1.f, 1.f);

		quad_vertices[3].normal = float3(0.f, 1.f, 0.1f);
		quad_vertices[3].uv = float2(0.f, 0.f);
		quad_vertices[3].color = float4(1.f, 1.f, 1.f, 1.f);

		StaticArray<Vertex> vertices;
		vertices.Init(a_temp_arena, m_map.size() * 4);
		StaticArray<uint32_t> indices;
		indices.Init(a_temp_arena, m_map.size() * 6);
		// optimize this
		for (int y = 0; y < m_map_size_y; y++)
		{
			for (int x = 0; x < m_map_size_x; x++)
			{
				const DungeonTile& tile = GetTile(x, y);
				if (tile.walkable)
				{
					const float fx = static_cast<float>(x);
					const float fy = static_cast<float>(y);
					const float3 pos_top_left = float3(fx - .5f, 0.f, fy + .5f);
					const float3 pos_top_right = float3(fx + .5f, 0.f, fy + .5f);
					const float3 pos_bot_right = float3(fx + .5f, 0.f, fy - .5f);
					const float3 pos_bot_left = float3(fx - .5f, 0.f, fy - .5f);

					quad_vertices[0].position = pos_top_left;
					quad_vertices[1].position = pos_top_right;
					quad_vertices[2].position = pos_bot_right;
					quad_vertices[3].position = pos_bot_left;

					const uint32_t current_index = vertices.size();
					const uint32_t quad_indices[] = {
						current_index,
						current_index + 1,
						current_index + 2,
						current_index + 2,
						current_index + 3,
						current_index 
					};
					vertices.push_back(quad_vertices.const_slice());
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
		map_obj = a_scene_hierarchy.CreateSceneObjectMesh(floor_pos, mesh_info, "dungeon map floor");
	}
	return map_obj;
}

static float3 RotatePointOnPoint(const float4x4& a_rotation_matrix, const float4x4& a_translate_matrix, const float3 a_point)
{
	const float4 res = a_rotation_matrix * float4(a_point, 1.f);
	const float4 res2 = a_translate_matrix * res;
	return float3(res2.x, res2.y, res2.z);
}

static void MakeWallSegment(StaticArray<Vertex>& a_vertices, StaticArray<uint32_t>& a_indices, QuadVertices& a_quad_vertices, const int a_x, const int a_y, const float3 a_offset, const float3 a_rotation)
{
	const float4x4 rotation_matrix = Float4x4FromRotation(Float3ToRadians(a_rotation));
	const float4x4 translate_matrix = Float4x4FromTranslation(float3(a_x, 0.5f, a_y));

	const float fx = static_cast<float>(a_x);
	const float fy = static_cast<float>(a_y);
	// rotate these
	const float3 pos_top_left = RotatePointOnPoint(rotation_matrix, translate_matrix, float3(fx - 0.5f, 0.5f, fy + 0.5f));
	const float3 pos_top_right = RotatePointOnPoint(rotation_matrix, translate_matrix, float3(fx + 0.5f, 0.5f, fy + 0.5f));
	const float3 pos_bot_right = RotatePointOnPoint(rotation_matrix, translate_matrix, float3(fx + 0.5f, 0.5f, fy - 0.5f));
	const float3 pos_bot_left = RotatePointOnPoint(rotation_matrix, translate_matrix, float3(fx - 0.5f, 0.5f, fy - 0.5f));

	a_quad_vertices[0].position = pos_top_left + a_offset;
	a_quad_vertices[1].position = pos_top_right + a_offset;
	a_quad_vertices[2].position = pos_bot_right + a_offset;
	a_quad_vertices[3].position = pos_bot_left + a_offset;

	const uint32_t current_index = a_vertices.size();
	const uint32_t quad_indices[] = {
		current_index,
		current_index + 1,
		current_index + 2,
		current_index + 2,
		current_index + 3,
		current_index
	};

	a_vertices.push_back(a_quad_vertices.const_slice());
	a_indices.push_back(quad_indices, _countof(quad_indices));
}

SceneObjectHandle DungeonMap::CreateSceneObjectWalls(MemoryArena& a_temp_arena, SceneHierarchy& a_scene_hierarchy, const float3 a_pos)
{
	QuadVertices quad_vertices;
	quad_vertices[0].normal = float3(0.f, 1.f, 0.1f);
	quad_vertices[0].uv = float2(0.f, 1.f);
	quad_vertices[0].color = float4(1.f, 1.f, 1.f, 1.f);

	quad_vertices[1].normal = float3(0.f, 1.f, 0.1f);
	quad_vertices[1].uv = float2(1.f, 1.f);
	quad_vertices[1].color = float4(1.f, 1.f, 1.f, 1.f);

	quad_vertices[2].normal = float3(0.f, 1.f, 0.1f);
	quad_vertices[2].uv = float2(1.f, 0.f);
	quad_vertices[2].color = float4(1.f, 1.f, 1.f, 1.f);

	quad_vertices[3].normal = float3(0.f, 1.f, 0.1f);
	quad_vertices[3].uv = float2(0.f, 0.f);
	quad_vertices[3].color = float4(1.f, 1.f, 1.f, 1.f);

	SceneObjectHandle map_obj{};
	MemoryArenaScope(a_temp_arena)
	{
		StaticArray<Vertex> vertices;
		vertices.Init(a_temp_arena, m_map.size() * 4);
		StaticArray<uint32_t> indices;
		indices.Init(a_temp_arena, m_map.size() * 6);

		for (int y = 0; y < m_map_size_y; y++)
		{
			for (int x = 0; x < m_map_size_x; x++)
			{
				const DungeonTile& tile = GetTile(x, y);
				if (tile.walkable)
				{
					if (!IsTileWalkable(x + 1, y))
					{
						MakeWallSegment(vertices, indices, quad_vertices, x, y, float3(0.5f, 0.f, 0.f), float3(0.f, 45.f, 0.f));
					}
					if (!IsTileWalkable(x - 1, y))
					{
						MakeWallSegment(vertices, indices, quad_vertices, x, y, float3(-0.5f, 0.f, 0.f), float3(45.f, 0.f, 0.f));
					}
					if (!IsTileWalkable(x, y + 1))
					{
						MakeWallSegment(vertices, indices, quad_vertices, x, y, float3(0.f, 0.f, 0.5f), float3(0.f, 0.f, 45.f));
					}
					if (!IsTileWalkable(x, y - 1))
					{
						MakeWallSegment(vertices, indices, quad_vertices, x, y, float3(0.f, 0.f, -0.5f), float3(0.f, 0.f, 0.f));
					}
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
		map_obj = a_scene_hierarchy.CreateSceneObjectMesh(a_pos, mesh_info, "dungeon map floor");
	}

	return map_obj;
}

float3 Player::Move(const float3 a_translation)
{
	return m_velocity = m_velocity + a_translation;
}

void Player::SetPosition(const float3 a_position)
{
	m_position = a_position;
}

void Player::SetVelocitySpeed(const float a_velocity_speed)
{
	m_velocity_speed = a_velocity_speed;
}

bool Player::Update(const float a_delta_time)
{
	const float3 velocity = m_velocity * m_velocity_speed * a_delta_time;
	m_velocity = m_velocity - velocity;
	m_position = m_position + velocity;
	return true;
}

float4x4 Player::CalculateView() const
{
	return Float4x4Lookat(m_position, m_position + m_forward, m_up);
}

bool DungeonGame::Init(const uint2 a_game_viewport_size, const uint32_t a_back_buffer_count)
{
	m_game_memory = MemoryArenaCreate();
	const uint32_t back_buffer_count = GetRenderIO().frame_count;
	m_scene_hierarchy.Init(m_game_memory, back_buffer_count, Asset::FindOrCreateString("game hierarchy"));
	m_scene_hierarchy.SetClearColor(float3(0.3f, 0.3f, 0.3f));

	m_viewport.Init(a_game_viewport_size, int2(0, 0), a_back_buffer_count, "game screen");

	DungeonRoom room;
	room.CreateRoom(m_game_memory, "../../resources/game/dungeon_rooms/map1.bmp");
	DungeonRoom* roomptr = &room;
	m_dungeon_map.CreateMap(m_game_memory, 30, 30, Slice(&roomptr, 1));
	const float3 map_start_pos = float3(0.f, 0.f, -10.f);
	MemoryArenaScope(m_game_memory)
	{
		m_dungeon_map.CreateSceneObjectFloor(m_game_memory, m_scene_hierarchy, map_start_pos);
		m_dungeon_map.CreateSceneObjectWalls(m_game_memory, m_scene_hierarchy, map_start_pos);
	}
	m_player.SetPosition(m_dungeon_map.GetSpawnPoint() + map_start_pos);
	m_player.SetVelocitySpeed(25.f);
	return true;
}

bool DungeonGame::Update(const float a_delta_time)
{
	m_player.Update(a_delta_time);

	if (m_free_cam.use_free_cam)
	{
		m_free_cam.camera.Update(a_delta_time);
		m_scene_hierarchy.SetView(m_free_cam.camera.CalculateView(), m_free_cam.camera.GetPosition());
	}
	else
	{
		m_scene_hierarchy.SetView(m_player.CalculateView(), m_player.GetPosition());
	}

	DisplayImGuiInfo();

	return true;
}

bool DungeonGame::HandleInput(const float a_delta_time, const Slice<InputEvent> a_input_events)
{
	for (size_t i = 0; i < a_input_events.size(); i++)
	{
		const InputEvent& ip = a_input_events[i];
		if (ip.input_type == INPUT_TYPE::KEYBOARD)
		{
			const KeyInfo& ki = ip.key_info;
			float3 player_move{};
			float player_rotate_y;
			if (ki.key_pressed)
			{
				switch (ki.scan_code)
				{
				case KEYBOARD_KEY::W:
					player_move.z = 1;
					break;
				case KEYBOARD_KEY::S:
					player_move.z = -1;
					break;
				case KEYBOARD_KEY::A:
					player_move.x = 1;
					break;
				case KEYBOARD_KEY::D:
					player_move.x = -1;
					break;
				case KEYBOARD_KEY::X:
					player_move.y = 1;
					break;
				case KEYBOARD_KEY::Z:
					player_move.y = -1;
					break;
				case KEYBOARD_KEY::Q:
					player_rotate_y = 1;
					break;
				case KEYBOARD_KEY::E:
					player_rotate_y = -1;
					break;
				case KEYBOARD_KEY::F:
					m_free_cam.freeze_free_cam = !m_free_cam.freeze_free_cam;
					m_free_cam.camera.SetVelocity();
					break;
				case KEYBOARD_KEY::G:
					ToggleFreeCam();
					break;
				default:
					break;
				}
			}
			if (m_free_cam.use_free_cam && !m_free_cam.freeze_free_cam)
			{
				const float swap_y = player_move.y;
				player_move.y = player_move.z;
				player_move.z = swap_y;
				player_move = player_move * a_delta_time;
				m_free_cam.camera.Move(player_move);
			}
			else
			{
				player_move.y = 0;
				m_player.Move(player_move);
			}
		}
		else if (ip.input_type == INPUT_TYPE::MOUSE)
		{
			const MouseInfo& mi = ip.mouse_info;
			const float2 mouse_move = (mi.move_offset * a_delta_time);

			if (mi.wheel_move)
			{
				m_free_cam.speed = Clampf(
					m_free_cam.speed + static_cast<float>(mi.wheel_move) * (m_free_cam.speed * 0.02f),
					m_free_cam.min_speed,
					m_free_cam.max_speed);
				m_free_cam.camera.SetSpeed(m_free_cam.speed);
			}

			if (m_free_cam.use_free_cam && !m_free_cam.freeze_free_cam)
			{
				m_free_cam.camera.Rotate(mouse_move.x, mouse_move.y);
			}
		}
	}

	return true;
}

// maybe ifdef this for editor
void DungeonGame::DisplayImGuiInfo()
{
	if (ImGui::Begin("game info"))
	{
		ImGui::Text("Use freecam: %s", m_free_cam.use_free_cam ? "true" : "false");
		if (ImGui::Button("Toggle freecam"))
		{
			ToggleFreeCam();
		}

		ImGui::Text("Freeze freecam: %s", m_free_cam.freeze_free_cam ? "true" : "false");
		if (ImGui::Button("Toggle freecam freeze"))
		{
			m_free_cam.freeze_free_cam = !m_free_cam.freeze_free_cam;
		}

		if (ImGui::SliderFloat("Freecam speed", &m_free_cam.speed, m_free_cam.min_speed, m_free_cam.max_speed))
		{
			 m_free_cam.camera.SetSpeed(m_free_cam.speed);
		}
		
		if (ImGui::CollapsingHeader("player"))
		{
			float velocity_speed = m_player.GetVelocitySpeed();
			if (ImGui::SliderFloat("velocity speed", &velocity_speed, 1.f, 100.f))
			{
				m_player.SetVelocitySpeed(velocity_speed);
			}
		}
	}
	ImGui::End();
}

void DungeonGame::Destroy()
{
	MemoryArenaFree(m_game_memory);
}

void DungeonGame::ToggleFreeCam()
{
	m_free_cam.use_free_cam = !m_free_cam.use_free_cam;
	m_free_cam.camera.SetPosition(m_player.GetPosition());
	m_free_cam.camera.SetUp(float3(0.f, 1.f, 0.f));
	m_free_cam.camera.SetSpeed(m_free_cam.speed);
}
