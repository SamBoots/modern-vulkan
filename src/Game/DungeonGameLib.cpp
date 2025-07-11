#include "DungeonGameLib.hpp"
#include "GameInstance.hpp"
#include "MaterialSystem.hpp"

#include "Math/Math.inl"

#include "lualib.h"
#include "lauxlib.h"
#include "lua/LuaTypes.hpp"

#include "OS/Program.h"

using namespace BB;

// basic dictonary6
constexpr int WALL_FIGURE = '#';
constexpr int FREE_FIGURE = '.';
constexpr int SPAWN_FIGURE = '@';

static size_t GetIFromXY(int a_x, int a_y, int a_max_x)
{
	return static_cast<size_t>(a_x + a_y * a_max_x);
}

struct DungeonRoom
{
	int size_x;
	int size_y;
	StaticArray<int> tiles;
};

static bool TileIsValid(const int a_value)
{
    return a_value == WALL_FIGURE || a_value == FREE_FIGURE || a_value == SPAWN_FIGURE;
}

static DungeonRoom CreateRoom(MemoryArena& a_temp_arena, const Buffer& a_buffer)
{
    const char* chars = reinterpret_cast<const char*>(a_buffer.data);

    int x = 0;
    int y = 0;
    int local_x = 0;
    for (size_t i = 0; i < a_buffer.size; i++)
    {
        const int value = chars[i];
        if (value == '\n')
        {
            if (y == 0)
                x = local_x;
            y++;
            BB_ASSERT(local_x == x, "map has unequal size_x on some dimensions");
            local_x = 0;
        }
        else if (TileIsValid(value))
            local_x++;
    }

    if (local_x > 0)
        y++;

    DungeonRoom room{};
	room.size_x = x;
	room.size_y = y;
	room.tiles = {};

	room.tiles.Init(a_temp_arena, static_cast<uint32_t>(x * y));

	for (size_t i = 0; i < a_buffer.size; i++)
	{
		const int value = chars[i];
		if (TileIsValid(value))
			room.tiles.push_back(value);
	}

	return room;
}

static ConstSlice<int> CreateMap(MemoryArena& a_temp_arena, const int a_map_size_x, const int a_map_size_y, const ConstSlice<DungeonRoom> a_rooms, int2& a_spawn_point)
{
    const uint32_t table_size = static_cast<uint32_t>(a_map_size_x * a_map_size_y);
    // walkable
	StaticArray<int> map = {};
	map.Init(a_temp_arena, table_size, table_size);
	map.fill(WALL_FIGURE);

    for (size_t i = 0; i < a_rooms.size(); i++)
    {
        const DungeonRoom& room = a_rooms[i];
        const int max_x = a_map_size_x - room.size_x;
        const int max_y = a_map_size_y - room.size_y;

        const int start_pos_x = static_cast<int>(Random::Random(1, static_cast<uint32_t>(max_x)));
        const int start_pos_y = static_cast<int>(Random::Random(1, static_cast<uint32_t>(max_y)));

        const int end_pos_x = start_pos_x + room.size_x;
        const int end_pos_y = start_pos_y + room.size_y;

        // try to load the map reverse so you don't need to track these two variables
        int room_x = 0;
        int room_y = 0;
        for (int y = start_pos_y; y < end_pos_y; y++)
        {
            for (int x = start_pos_x; x < end_pos_x; x++)
            {
                const int tile = room.tiles[GetIFromXY(room_x++, room_y, room.size_x)];
				
                if (TileIsValid(tile))
                {
					map[GetIFromXY(x, y, a_map_size_x)] = tile;
					if (tile == SPAWN_FIGURE)
						a_spawn_point = int2(x, y);
                }
                else
                {
                    BB_ASSERT(false, "unidentified tile index");
                }
            }
            room_x = 0;
            ++room_y;
        }
    }

	return map.const_slice();
}

using QuadVerticesPos = FixedArray<float3, 4>;
using QuadVerticesNormals = FixedArray<float3, 4>;
using QuadVerticesUVs = FixedArray<float2, 4>;
using QuadVerticesColors = FixedArray<float4, 4>;

static ECSEntity CreateMapFloor(MemoryArena& a_temp_arena, const ConstSlice<int> a_map, const int a_size_x, const int a_size_y, SceneHierarchy& a_scene_hierarchy, const ECSEntity a_parent)
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
		positions.Init(a_temp_arena, static_cast<uint32_t>(a_map.size() * 4));
		normals.Init(a_temp_arena, static_cast<uint32_t>(a_map.size() * 4));
		uvs.Init(a_temp_arena, static_cast<uint32_t>(a_map.size() * 4));
		colors.Init(a_temp_arena, static_cast<uint32_t>(a_map.size() * 4));

		float max_x = 0;
		float max_y = 0;

		StaticArray<uint32_t> indices;
		indices.Init(a_temp_arena, static_cast<uint32_t>(a_map.size() * 6));
		// optimize this
		for (int y = 0; y < a_size_y; y++)
		{
			for (int x = 0; x < a_size_x; x++)
			{
				const int& tile = a_map[GetIFromXY(x, y, a_size_x)];
				if (tile != WALL_FIGURE)
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

static bool IsTileWall(const ConstSlice<int> a_map, const int a_x, const int a_y, const int a_max_x, const int a_max_y)
{
	if (a_x >= a_max_x || a_y >= a_max_y || 0 > a_x || 0 > a_y)
		return false;

	const int& tile = a_map[GetIFromXY(a_x, a_y, a_max_x)];
	return tile == WALL_FIGURE;
}

static ECSEntity CreateMapWalls(MemoryArena& a_temp_arena, const ConstSlice<int> a_map, const int a_size_x, const int a_size_y, SceneHierarchy& a_scene_hierarchy, const ECSEntity a_parent)
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
        StaticArray<float3> positions{};
        StaticArray<float3> normals{};
        StaticArray<float2> uvs{};
        StaticArray<float4> colors{};
		positions.Init(a_temp_arena, static_cast<uint32_t>(a_map.size() * 4));
		normals.Init(a_temp_arena, static_cast<uint32_t>(a_map.size() * 4));
		uvs.Init(a_temp_arena, static_cast<uint32_t>(a_map.size() * 4));
		colors.Init(a_temp_arena, static_cast<uint32_t>(a_map.size() * 4));
		StaticArray<uint32_t> indices;
		indices.Init(a_temp_arena, static_cast<uint32_t>(a_map.size() * 6));

		auto MakeWallSegment = [&](const int a_x, const int a_y, const float3 a_offset, const float3 a_rotation)
			{
				const float fx = static_cast<float>(a_x);
				const float fz = static_cast<float>(a_y);
				const float3 middle = float3(fx, 0.5f, fz);
				const float3x3 rotation_matrix = Float3x3FromRotation(Float3ToRadians(a_rotation));

				// rotate these
				const float3 pos_top_left = Float3RotatePoint(rotation_matrix, float3(fx - 0.5f, 0.5f, fz + 0.5f), middle);
				const float3 pos_top_right = Float3RotatePoint(rotation_matrix, float3(fx + 0.5f, 0.5f, fz + 0.5f), middle);
				const float3 pos_bot_right = Float3RotatePoint(rotation_matrix, float3(fx + 0.5f, 0.5f, fz - 0.5f), middle);
				const float3 pos_bot_left = Float3RotatePoint(rotation_matrix, float3(fx - 0.5f, 0.5f, fz - 0.5f), middle);

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

		for (int y = 0; y < a_size_y; y++)
		{
			for (int x = 0; x < a_size_x; x++)
			{
				const int& tile = a_map[GetIFromXY(x, y, a_size_x)];
				if (tile != WALL_FIGURE)
				{
					if (IsTileWall(a_map, x + 1, y, a_size_x, a_size_y))
						MakeWallSegment(x, y, float3(0.5f, 0.f, 0.f), float3(0.f, 0.f, 90.f));
					if (IsTileWall(a_map, x - 1, y, a_size_x, a_size_y))
						MakeWallSegment(x, y, float3(-0.5f, 0.f, 0.f), float3(0.f, 0.f, -90.f));
					if (IsTileWall(a_map, x, y + 1, a_size_x, a_size_y))
						MakeWallSegment(x, y, float3(0.f, 0.f, 0.5f), float3(-90.f, 0.f, 0.f));
					if (IsTileWall(a_map, x, y - 1, a_size_x, a_size_y))
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

static GameInstance* GetGameInstance(lua_State* a_state)
{
    return reinterpret_cast<GameInstance*>(lua_touserdata(a_state, lua_upvalueindex(1)));
}

static int CreateMapTilesFromFiles(lua_State* a_state)
{
    GameInstance* inst = GetGameInstance(a_state);

	const int map_size_x = lua_tointeger(a_state, 1);
	const int map_size_y = lua_tointeger(a_state, 2);

	if (!lua_istable(a_state, 3))
		return 0;

	const uint32_t file_count = lua_rawlen(a_state, 3);

	ECSEntity parent = inst->GetSceneHierarchy().CreateEntity(float3(0, 0, 0), "Dungeon Map");
    int2 spawn_point = {};

	MemoryArenaScope(inst->GetMemory())
	{
		StaticArray<DungeonRoom> rooms = {};
		rooms.Init(inst->GetMemory(), file_count);

		for (int i = 1; i < static_cast<int>(file_count + 1); i++)
		{
			lua_rawgeti(a_state, 3, i);
			if (!lua_isstring(a_state, -1))
				return 0;

			const char* file_name = lua_tostring(a_state, -1);
			PathString level_path = inst->GetProjectPath();
			level_path.AddPathNoSlash(file_name);
			const Buffer buffer = OSReadFile(inst->GetMemory(), level_path.c_str());
			DungeonRoom room = CreateRoom(inst->GetMemory(), buffer);
			rooms.emplace_back(room);

			lua_pop(a_state, 1);
		}

		// generate the dungeon
		ConstSlice<int> map = CreateMap(inst->GetMemory(), map_size_x, map_size_y, rooms.const_slice(), spawn_point);
        CreateMapFloor(inst->GetMemory(), map, map_size_x, map_size_y, inst->GetSceneHierarchy(), parent);
		CreateMapWalls(inst->GetMemory(), map, map_size_x, map_size_y, inst->GetSceneHierarchy(), parent);

        const int table_size = static_cast<int>(map.size());
		lua_createtable(a_state, table_size + 1, 0);
		for (int i = 0; i < table_size; i++)
		{
			lua_pushinteger(a_state, map[static_cast<size_t>(i)]);
			lua_rawseti(a_state, -2, i + 1);
		}
	}

	lua_pushinteger(a_state, spawn_point.x);
	lua_pushinteger(a_state, spawn_point.y);
	lua_pushbbhandle(a_state, parent.handle);
    return 4;
}

// todo , fix this to be better
static void RegisterLuaFunction(lua_State* a_state, const lua_CFunction a_function, const char* a_func_name)
{
    lua_pushvalue(a_state, -1);
    lua_pushcclosure(a_state, a_function, 1);
    lua_setglobal(a_state, a_func_name);
}

void BB::RegisterDungeonGameLibLuaFunctions(GameInstance& a_inst)
{
    const int top = lua_gettop(a_inst.GetLuaState());
    lua_pushlightuserdata(a_inst.GetLuaState(), &a_inst);
    RegisterLuaFunction(a_inst.GetLuaState(), CreateMapTilesFromFiles, "CreateMapTilesFromFiles");
    lua_settop(a_inst.GetLuaState(), top);
}
