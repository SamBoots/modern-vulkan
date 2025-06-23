#include "GameMain.hpp"
#include "imgui.h"
#include "Program.h"
#include "SceneHierarchy.hpp"

#include "InputSystem.hpp"

#include "lua.h"
#include "lauxlib.h"
#include "lua/LuaTypes.hpp"

#include "Engine.hpp"
#include "JsonLoading.hpp"

using namespace BB;

bool DungeonGame::Init(const uint2 a_game_viewport_size, const uint32_t a_back_buffer_count, const StringView a_project_name)
{
	m_game_memory = MemoryArenaCreate();

    m_project_path = GetRootPath();
    m_project_path.append("\\projects\\");
    m_project_path.append(a_project_name);
    m_project_path.push_directory_slash();

    {
        PathString input_path = m_project_path;
        input_path.append("input.json");
        m_input_channel = CreateInputChannelByJson(m_game_memory, a_project_name, input_path.GetView());
    }

	m_scene_hierarchy.Init(m_game_memory, STANDARD_ECS_OBJ_COUNT, a_game_viewport_size, a_back_buffer_count, a_project_name);

	m_viewport.Init(a_game_viewport_size, int2(0, 0), "dungeon game viewport");

    PathString room_path = m_project_path;
    room_path.append("rooms/map1.bmp");

	DungeonRoom room;
	room.CreateRoom(m_game_memory, room_path.c_str());
	DungeonRoom* roomptr = &room;
	m_dungeon_map.CreateMap(m_game_memory, 30, 30, Slice(&roomptr, 1));
	const float3 map_start_pos = float3(0.f, 0.f, -10.f);

	m_dungeon_obj = m_scene_hierarchy.CreateEntity(map_start_pos, "dungeon map");
	MemoryArenaScope(m_game_memory)
	{
		m_dungeon_map.CreateEntityFloor(m_game_memory, m_scene_hierarchy, m_dungeon_obj);
		m_dungeon_map.CreateEntityWalls(m_game_memory, m_scene_hierarchy, m_dungeon_obj);
	}

    m_context.Init(m_game_memory, m_input_channel, &m_scene_hierarchy.GetECS(), gbSize);
    m_context.RegisterActionHandlesLua(m_input_channel);

    PathString lua_path = m_project_path;
    lua_path.append("lua\\");
    MemoryArenaScope(m_game_memory)
    {
        bool status = m_context.LoadLuaDirectory(m_game_memory, lua_path.GetView());
        BB_ASSERT(status, "something went wrong loading a lua directory");
    }

    lua_getglobal(m_context.GetState(), "Init");
    lua_pushfloat3(m_context.GetState(), m_dungeon_map.GetSpawnPoint() + map_start_pos + float3(0.f, 1.f, 0.f));
    BB_ASSERT(lua_pcall(m_context.GetState(), 1, 0, 0) == LUA_OK, lua_tostring(m_context.GetState(), -1));

	return true;
}

bool DungeonGame::Update(const float a_delta_time, const bool a_selected)
{
    const LuaStackScope scope(m_context.GetState());

    lua_getglobal(m_context.GetState(), "Update");
    lua_pushnumber(m_context.GetState(), static_cast<lua_Number>(a_delta_time));
    lua_pushboolean(m_context.GetState(), a_selected);
    BB_ASSERT(lua_pcall(m_context.GetState(), 2, 0, 0) == LUA_OK, lua_tostring(m_context.GetState(), -1));

    const float3 pos = GetCameraPos();
    const float4x4 view = GetCameraView();
    m_scene_hierarchy.GetECS().GetRenderSystem().SetView(view, pos);


	DisplayImGuiInfo();

	return true;
}

// maybe ifdef this for editor
void DungeonGame::DisplayImGuiInfo()
{
	//if (ImGui::Begin("game info"))
	//{
	//	ImGui::Text("Use freecam: %s", m_free_cam.use_free_cam ? "true" : "false");
	//	if (ImGui::Button("Toggle freecam"))
	//	{
	//		ToggleFreeCam();
	//	}

	//	if (ImGui::SliderFloat("Freecam speed", &m_free_cam.speed, m_free_cam.min_speed, m_free_cam.max_speed))
	//	{
	//		 m_free_cam.camera.SetSpeed(m_free_cam.speed);
	//	}
	//	
	//	if (ImGui::CollapsingHeader("player"))
	//	{
	//		float velocity_speed = m_player.GetLerpSpeed();
	//		if (ImGui::SliderFloat("velocity speed", &velocity_speed, 1.f, 100.f))
	//		{
	//			m_player.SetLerpSpeed(velocity_speed);
	//		}
	//	}
	//}
	//ImGui::End();
}

void DungeonGame::Destroy()
{
	MemoryArenaFree(m_game_memory);
}

float3 DungeonGame::GetCameraPos()
{
    const LuaStackScope scope(m_context.GetState());
    lua_getglobal(m_context.GetState(), "GetCameraPos");
    lua_pcall(m_context.GetState(), 0, 1, 0);
    return *lua_getfloat3(m_context.GetState(), -1);
}

float4x4 DungeonGame::GetCameraView()
{
    const float3 pos = GetCameraPos();
    lua_getglobal(m_context.GetState(), "GetCameraUp");
    BB_ASSERT(lua_pcall(m_context.GetState(), 0, 1, 0) == LUA_OK, lua_tostring(m_context.GetState(), -1));
    const float3 up = *lua_getfloat3(m_context.GetState(), -1);
    lua_getglobal(m_context.GetState(), "GetCameraForward");
    BB_ASSERT(lua_pcall(m_context.GetState(), 0, 1, 0) == LUA_OK, lua_tostring(m_context.GetState(), -1));
    const float3 forward = *lua_getfloat3(m_context.GetState(), -1);

    return Float4x4Lookat(pos, pos + forward, up);
}
