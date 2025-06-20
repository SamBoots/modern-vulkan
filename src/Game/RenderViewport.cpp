#include "RenderViewport.hpp"
#include "BBjson.hpp"
#include "Program.h"
#include "HID.h"
#include "AssetLoader.hpp"
#include "Engine.hpp"
#include "JsonLoading.hpp"

#include "Math/Math.inl"
#include "Math/Collision.inl"

#include "InputSystem.hpp"

#include "lua.h"
#include "lauxlib.h"
#include "lua/LuaTypes.hpp"

using namespace BB;

bool RenderViewport::Init(const uint2 a_game_viewport_size, const uint32_t a_back_buffer_count, const StringView a_project_name)
{
	m_memory = MemoryArenaCreate();

    m_project_path = GetRootPath();
    m_project_path.append("\\projects\\");
    m_project_path.append(a_project_name);
    m_project_path.push_directory_slash();

    {
        PathString input_path = m_project_path;
        input_path.append("input.json");
        m_input_channel = CreateInputChannelByJson(m_memory, a_project_name, input_path.GetView());
    }

    m_scene_hierarchy.Init(m_memory, STANDARD_ECS_OBJ_COUNT, a_game_viewport_size, a_back_buffer_count, a_project_name);

    {
        PathString scene_path = m_project_path;
        scene_path.append("scene.json");
	    JsonParser json_file(scene_path.c_str());
	    json_file.Parse();
        MemoryArenaScope(m_memory)
        {
            auto viewer_list = SceneHierarchy::PreloadAssetsFromJson(m_memory, json_file);

            Asset::LoadAssets(m_memory, viewer_list.slice());
        }
        CreateEntitiesViaJson(m_scene_hierarchy, scene_path.GetView());
    }

	m_viewport.Init(a_game_viewport_size, int2(0, 0), a_project_name);

    m_context.Init(m_memory, m_input_channel,&m_scene_hierarchy.GetECS(), gbSize);
    m_context.RegisterActionHandlesLua(m_input_channel);

    PathString lua_path = m_project_path;
    lua_path.append("lua\\");
    MemoryArenaScope(m_memory)
    {
        bool status = m_context.LoadLuaDirectory(m_memory, lua_path.GetView());
        BB_ASSERT(status, "something went wrong loading a lua directory");
    }
	return true;
}

bool RenderViewport::Update(const float a_delta_time, const bool a_selected)
{
    const LuaStackScope scope(m_context.GetState());

    lua_getglobal(m_context.GetState(), "Update");
    lua_pushnumber(m_context.GetState(), static_cast<lua_Number>(a_delta_time));
    lua_pushboolean(m_context.GetState(), a_selected);
    BB_ASSERT(lua_pcall(m_context.GetState(), 2, 0, 0) == LUA_OK, lua_tostring(m_context.GetState(), -1));

    const float3 pos = GetCameraPos();
    const float4x4 view = GetCameraView();
	m_scene_hierarchy.GetECS().GetRenderSystem().SetView(view, pos);

	return true;
}

float3 RenderViewport::GetCameraPos()
{
    const LuaStackScope scope(m_context.GetState());
    lua_getglobal(m_context.GetState(), "GetCameraPos");
    lua_pcall(m_context.GetState(), 0, 1, 0);
    return *lua_getfloat3(m_context.GetState(), -1);
}

float4x4 RenderViewport::GetCameraView()
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

void RenderViewport::Destroy()
{

}
