#include "DungeonGameLib.hpp"
#include "MaterialSystem.hpp"
#include "BBImage.hpp"
#include "SceneHierarchy.hpp"

#include "Math/Math.inl"

#include "lualib.h"
#include "lauxlib.h"

#include "OS/Program.h"

using namespace BB;

// basic dictonary
constexpr int WALL_FIGURE = '#';
constexpr int FREE_FIGURE = '.';
constexpr int SPAWN_FIGURE = '@';

static bool TileIsValid(const int a_value)
{
    return a_value == WALL_FIGURE || a_value == FREE_FIGURE || a_value == SPAWN_FIGURE;
}

static void ReadMapBufferToLuaArray(lua_State* a_state, const Buffer& a_buffer, int2& a_size)
{
    lua_newtable(a_state);

    const char* chars = reinterpret_cast<const char*>(a_buffer.data);

    int x = 0;
    int y = 0;
    int local_x = 0;
    for (size_t i = 0; i < a_buffer.size; i++)
    {
        const int value = chars[i];
        if (TileIsValid(value))
        {
            lua_pushinteger(a_state, value);
            lua_rawseti(a_state, -2, i + 1);
        }
        else
        {
            if (value == '\n')
            {
                if (y == 0)
                    x = local_x;
                ++y;
                BB_ASSERT(local_x == x, "map has unequal size_x on some dimensions");
            }
        }
        local_x++;
    }

    a_size.x = x;
    a_size.y = y;
}

static MemoryArena* GetMemoryArena(lua_State* a_state, int a_upvalueindex)
{
    return reinterpret_cast<MemoryArena*>(lua_touserdata(a_state, lua_upvalueindex(a_upvalueindex)));
}

static int CreateMapTilesFromFile(lua_State* a_state)
{
    MemoryArena* arena = GetMemoryArena(a_state, 1);

    const char* str = lua_tostring(a_state, 1);
    PathString level_path = /* project path*/ str;
    int2 size;
    MemoryArenaScope(*arena)
    {
        const Buffer buffer = OSReadFile(*arena, level_path.c_str());
        ReadMapBufferToLuaArray(a_state, buffer, size);
    }

    // do -2 here?
    lua_setmetatable(a_state, -2);
    lua_pushinteger(a_state, size.x);
    lua_pushinteger(a_state, size.y);
    // push table

    return 3;
}

// todo , fix this to be better
static void RegisterLuaFunction(lua_State* a_state, const lua_CFunction a_function, const char* a_func_name)
{
    lua_pushvalue(a_state, -1);
    lua_pushcclosure(a_state, a_function, 1);
    lua_setglobal(a_state, a_func_name);
}

void BB::RegisterDungeonGameLibLuaFunctions(lua_State* a_state, MemoryArena* a_arena)
{
    const int top = lua_gettop(a_state);
    lua_pushlightuserdata(a_state, a_arena);
    RegisterLuaFunction(a_state, CreateMapTilesFromFile, "CreateMapTilesFromFile");

    lua_settop(a_state, top);
}

using QuadVerticesPos = FixedArray<float3, 4>;
using QuadVerticesNormals = FixedArray<float3, 4>;
using QuadVerticesUVs = FixedArray<float2, 4>;
using QuadVerticesColors = FixedArray<float4, 4>;

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
		const Color* pixels = reinterpret_cast<const Color*>(image.GetPixels());

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
				const size_t index = static_cast<size_t>(GetMapIndexFromXY(x, y));

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

ECSEntity DungeonMap::CreateEntityFloor(MemoryArena& a_temp_arena, SceneHierarchy& a_scene_hierarchy, const ECSEntity a_parent)
{
	ECSEntity map_obj{};
	MemoryArenaScope(a_temp_arena)
	{
		QuadVerticesNormals quad_norms;
		quad_norms[0] = float3(0.f, 1.f, 0.1f);
		quad_norms[1] = float3(0.f, 1.f, 0.1f);
		quad_norms[2] = float3(0.f, 1.f, 0.1f);
		quad_norms[3] = float3(0.f, 1.f, 0.1f);

		QuadVerticesUVs quad_uvs;
		quad_uvs[0] = float2(0.f, 1.f);
		quad_uvs[1] = float2(1.f, 1.f);
		quad_uvs[2] = float2(1.f, 0.f);
		quad_uvs[3] = float2(0.f, 0.f);

		QuadVerticesColors quad_colors;
		quad_colors[0] = float4(1.f, 1.f, 1.f, 1.f);
		quad_colors[1] = float4(1.f, 1.f, 1.f, 1.f);
		quad_colors[2] = float4(1.f, 1.f, 1.f, 1.f);
		quad_colors[3] = float4(1.f, 1.f, 1.f, 1.f);

		StaticArray<float3> positions;
		StaticArray<float3> normals;
		StaticArray<float2> uvs;
		StaticArray<float4> colors;
		positions.Init(a_temp_arena, m_map.size() * 4);
		normals.Init(a_temp_arena, m_map.size() * 4);
		uvs.Init(a_temp_arena, m_map.size() * 4);
		colors.Init(a_temp_arena, m_map.size() * 4);

		float max_x = 0;
		float max_y = 0;

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
					QuadVerticesPos quad_pos;
					const float fx = static_cast<float>(x);
					const float fy = static_cast<float>(y);
					const float3 pos_top_left = float3(fx - .5f, 0.f, fy + .5f);
					const float3 pos_top_right = float3(fx + .5f, 0.f, fy + .5f);
					const float3 pos_bot_right = float3(fx + .5f, 0.f, fy - .5f);
					const float3 pos_bot_left = float3(fx - .5f, 0.f, fy - .5f);

					max_x = Max(max_x, fx);
					max_y = Max(max_y, fy);

					quad_pos[0] = pos_top_left;
					quad_pos[1] = pos_top_right;
					quad_pos[2] = pos_bot_right;
					quad_pos[3] = pos_bot_left;

					const uint32_t current_index = positions.size();
					const uint32_t quad_indices[] = {
						current_index,
						current_index + 1,
						current_index + 2,
						current_index + 2,
						current_index + 3,
						current_index
					};
					positions.push_back(quad_pos.const_slice());
					normals.push_back(quad_norms.const_slice());
					uvs.push_back(quad_uvs.const_slice());
					colors.push_back(quad_colors.const_slice());
					indices.push_back(quad_indices, _countof(quad_indices));
				}
			}
		}

		Asset::MeshLoadFromMemory load_mesh_info;
		load_mesh_info.name = "dungeon floor";
		load_mesh_info.indices = indices.const_slice();
		load_mesh_info.base_albedo = Asset::GetCheckerBoardTexture();
		load_mesh_info.mesh_load.positions = positions.const_slice();
		load_mesh_info.mesh_load.normals = normals.const_slice();
		load_mesh_info.mesh_load.uvs = uvs.const_slice();
		load_mesh_info.mesh_load.colors = colors.const_slice();
		Mesh mesh = Asset::LoadMeshFromMemory(a_temp_arena, load_mesh_info).meshes[0].mesh;
		MeshMetallic material_info;
		material_info.metallic_factor = 1.0f;
		material_info.roughness_factor = 0.0f;
		material_info.base_color_factor = float4(1.f);
		material_info.albedo_texture = Asset::GetCheckerBoardTexture();
		material_info.normal_texture = Asset::GetBlueTexture();
		material_info.orm_texture = Asset::GetWhiteTexture();

		BoundingBox bounding_box;
		bounding_box.min = float3(0.f);
		bounding_box.max.x = max_x;
		bounding_box.max.y = max_y;
		bounding_box.max.z = 1.f;

		SceneMeshCreateInfo mesh_info;
		mesh_info.mesh = mesh;
		mesh_info.index_start = 0;
		mesh_info.index_count = indices.size();
		mesh_info.master_material = Material::GetDefaultMasterMaterial(PASS_TYPE::SCENE, MATERIAL_TYPE::MATERIAL_3D);
		mesh_info.material_data = material_info;
		map_obj = a_scene_hierarchy.CreateEntityMesh(float3(), mesh_info, "dungeon map floor", bounding_box, a_parent);
	}
	return map_obj;
}

static float3 RotatePointOnPoint(const float3x3& a_rotation_matrix, const float3 a_point, const float3 a_middle)
{
	const float3 res = a_rotation_matrix * (a_point - a_middle);
	return a_middle + res;
}

ECSEntity DungeonMap::CreateEntityWalls(MemoryArena& a_temp_arena, SceneHierarchy& a_scene_hierarchy, const ECSEntity a_parent)
{
	QuadVerticesNormals quad_norms;
	quad_norms[0] = float3(0.f, 1.f, 0.1f);
	quad_norms[1] = float3(0.f, 1.f, 0.1f);
	quad_norms[2] = float3(0.f, 1.f, 0.1f);
	quad_norms[3] = float3(0.f, 1.f, 0.1f);

	QuadVerticesUVs quad_uvs;
	quad_uvs[0] = float2(0.f, 1.f);
	quad_uvs[1] = float2(1.f, 1.f);
	quad_uvs[2] = float2(1.f, 0.f);
	quad_uvs[3] = float2(0.f, 0.f);

	QuadVerticesColors quad_colors;
	quad_colors[0] = float4(1.f, 1.f, 1.f, 1.f);
	quad_colors[1] = float4(1.f, 1.f, 1.f, 1.f);
	quad_colors[2] = float4(1.f, 1.f, 1.f, 1.f);
	quad_colors[3] = float4(1.f, 1.f, 1.f, 1.f);

	float max_x = 0;
	float max_z = 0;

	ECSEntity map_obj{};
	MemoryArenaScope(a_temp_arena)
	{
		StaticArray<float3> positions;
		StaticArray<float3> normals;
		StaticArray<float2> uvs;
		StaticArray<float4> colors;
		positions.Init(a_temp_arena, m_map.size() * 4);
		normals.Init(a_temp_arena, m_map.size() * 4);
		uvs.Init(a_temp_arena, m_map.size() * 4);
		colors.Init(a_temp_arena, m_map.size() * 4);
		StaticArray<uint32_t> indices;
		indices.Init(a_temp_arena, m_map.size() * 6);

		auto MakeWallSegment = [&](const int a_x, const int a_y, const float3 a_offset, const float3 a_rotation)
			{
				const float fx = static_cast<float>(a_x);
				const float fz = static_cast<float>(a_y);
				const float3 middle = float3(fx, 0.5f, fz);
				const float3x3 rotation_matrix = Float3x3FromRotation(Float3ToRadians(a_rotation));

				// rotate these
				const float3 pos_top_left = RotatePointOnPoint(rotation_matrix, float3(fx - 0.5f, 0.5f, fz + 0.5f), middle);
				const float3 pos_top_right = RotatePointOnPoint(rotation_matrix, float3(fx + 0.5f, 0.5f, fz + 0.5f), middle);
				const float3 pos_bot_right = RotatePointOnPoint(rotation_matrix, float3(fx + 0.5f, 0.5f, fz - 0.5f), middle);
				const float3 pos_bot_left = RotatePointOnPoint(rotation_matrix, float3(fx - 0.5f, 0.5f, fz - 0.5f), middle);

				max_x = Max(max_x, fx);
				max_z = Max(max_z, fz);

				QuadVerticesPos pos;
				pos[0] = pos_top_left + a_offset;
				pos[1] = pos_top_right + a_offset;
				pos[2] = pos_bot_right + a_offset;
				pos[3] = pos_bot_left + a_offset;

				const uint32_t current_index = positions.size();
				const uint32_t quad_indices[] = {
					current_index,
					current_index + 1,
					current_index + 2,
					current_index + 2,
					current_index + 3,
					current_index
				};
				positions.push_back(pos.const_slice());
				normals.push_back(quad_norms.const_slice());
				uvs.push_back(quad_uvs.const_slice());
				colors.push_back(quad_colors.const_slice());
				indices.push_back(quad_indices, _countof(quad_indices));
			};

		for (int y = 0; y < m_map_size_y; y++)
		{
			for (int x = 0; x < m_map_size_x; x++)
			{
				const DungeonTile& tile = GetTile(x, y);
				if (tile.walkable)
				{
					if (!IsTileWalkable(x + 1, y))
						MakeWallSegment(x, y, float3(0.5f, 0.f, 0.f), float3(0.f, 0.f, 90.f));
					if (!IsTileWalkable(x - 1, y))
						MakeWallSegment(x, y, float3(-0.5f, 0.f, 0.f), float3(0.f, 0.f, -90.f));
					if (!IsTileWalkable(x, y + 1))
						MakeWallSegment(x, y, float3(0.f, 0.f, 0.5f), float3(-90.f, 0.f, 0.f));
					if (!IsTileWalkable(x, y - 1))
						MakeWallSegment(x, y, float3(0.f, 0.f, -0.5f), float3(90.f, 0.f, 0.f));
				}
			}
		}

		BoundingBox bounding_box;
		bounding_box.min = float3(0.f);
		bounding_box.max.x = max_x;
		bounding_box.max.y = 1.f;
		bounding_box.max.z = max_z;

		Asset::MeshLoadFromMemory load_mesh_info;
		load_mesh_info.name = "dungeon wall";
		load_mesh_info.indices = indices.const_slice();
		load_mesh_info.base_albedo = Asset::GetCheckerBoardTexture();
		load_mesh_info.mesh_load.positions = positions.const_slice();
		load_mesh_info.mesh_load.normals = normals.const_slice();
		load_mesh_info.mesh_load.uvs = uvs.const_slice();
		load_mesh_info.mesh_load.colors = colors.const_slice();
		Mesh mesh = Asset::LoadMeshFromMemory(a_temp_arena, load_mesh_info).meshes[0].mesh;
		MeshMetallic material_info;
		material_info.metallic_factor = 1.0f;
		material_info.roughness_factor = 0.0f;
		material_info.base_color_factor = float4(1.f);
		material_info.albedo_texture = Asset::GetCheckerBoardTexture();
		material_info.normal_texture = Asset::GetBlueTexture();
		material_info.orm_texture = RDescriptorIndex(0);

		SceneMeshCreateInfo mesh_info;
		mesh_info.mesh = mesh;
		mesh_info.index_start = 0;
		mesh_info.index_count = indices.size();
		mesh_info.master_material = Material::GetDefaultMasterMaterial(PASS_TYPE::SCENE, MATERIAL_TYPE::MATERIAL_3D);
		mesh_info.material_data = material_info;
		map_obj = a_scene_hierarchy.CreateEntityMesh(float3(), mesh_info, "dungeon map wall", bounding_box, a_parent);
	}

	return map_obj;
}
