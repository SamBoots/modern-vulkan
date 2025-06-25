#include "GameInstance.hpp"
#include "Engine.hpp"
#include "InputSystem.hpp"

#include "lua/LuaTypes.hpp"
#include "lua/LuaECSApi.hpp"

using namespace BB;

bool GameInstance::Init(const uint2 a_game_viewport_size, const StringView a_project_name, MemoryArena* a_parena)
{
    if (a_parena)
        m_arena = *a_parena;
    else
        m_arena = MemoryArenaCreate();

    m_project_path = GetRootPath();
    m_project_path.append("\\projects\\");
    m_project_path.append(a_project_name);
    m_project_path.push_directory_slash();

    {
        PathString input_path = m_project_path;
        input_path.append("input.json");
        m_input_channel = Input::CreateInputChannelByJson(m_arena, a_project_name, input_path.GetView());
    }

    m_scene_hierarchy.Init(m_arena, STANDARD_ECS_OBJ_COUNT, a_game_viewport_size, a_project_name);

    m_viewport.Init(a_game_viewport_size, int2(0, 0), a_project_name);

    m_lua.Init(m_arena, gbSize);
    RegisterLuaCFunctions();
    m_lua.RegisterActionHandlesLua(m_input_channel);

    PathString lua_path = m_project_path;
    lua_path.append("lua\\");
    MemoryArenaScope(m_arena)
    {
        bool status = m_lua.LoadLuaDirectory(m_arena, lua_path.GetView());
        BB_ASSERT(status, "something went wrong loading a lua directory");
    }

    if (!Verify())
        return false;

    lua_getglobal(m_lua.State(), "Init");
    int status = lua_pcall(m_lua.State(), 0, 0, 0);
    BB_ASSERT(status == LUA_OK, lua_tostring(m_lua.State(), -1));

    return true;
}

bool GameInstance::Update(const float a_delta_time, const bool a_selected)
{
    const LuaStackScope scope(m_lua.State());

    lua_getglobal(m_lua.State(), "Update");
    lua_pushnumber(m_lua.State(), static_cast<lua_Number>(a_delta_time));
    lua_pushboolean(m_lua.State(), a_selected);
    int status = lua_pcall(m_lua.State(), 2, 0, 0);
    BB_ASSERT(status == LUA_OK, lua_tostring(m_lua.State(), -1));

    const float3 pos = GetCameraPos();
    const float4x4 view = GetCameraView();
    m_scene_hierarchy.GetECS().GetRenderSystem().SetView(view, pos);
    return true;
}

void GameInstance::Destroy()
{

}

float3 GameInstance::GetCameraPos()
{
    const LuaStackScope scope(m_lua.State());
    lua_getglobal(m_lua.State(), "GetCameraPos");
    lua_pcall(m_lua.State(), 0, 1, 0);
    return *lua_getfloat3(m_lua.State(), -1);
}
float4x4 GameInstance::GetCameraView()
{
    const float3 pos = GetCameraPos();
    lua_getglobal(m_lua.State(), "GetCameraUp");
    BB_ASSERT(lua_pcall(m_lua.State(), 0, 1, 0) == LUA_OK, lua_tostring(m_lua.State(), -1));
    const float3 up = *lua_getfloat3(m_lua.State(), -1);
    lua_getglobal(m_lua.State(), "GetCameraForward");
    BB_ASSERT(lua_pcall(m_lua.State(), 0, 1, 0) == LUA_OK, lua_tostring(m_lua.State(), -1));
    const float3 forward = *lua_getfloat3(m_lua.State(), -1);

    return Float4x4Lookat(pos, pos + forward, up);
}

bool GameInstance::Verify()
{
    const LuaStackScope scope(m_lua.State());
    int status = lua_getglobal(m_lua.State(), "Init");
    if (status == LUA_TNIL)
    {
        BB_WARNING(false, "lua couldn't get global function Init", WarningType::HIGH);
        return false;
    }
    status = lua_getglobal(m_lua.State(), "Update");
    if (status == LUA_TNIL)
    {
        BB_WARNING(false, "lua couldn't get global function Update", WarningType::HIGH);
        return false;
    }
    status = lua_getglobal(m_lua.State(), "GetCameraPos");
    if (status == LUA_TNIL)
    {
        BB_WARNING(false, "lua couldn't get global function GetCameraPos", WarningType::HIGH);
        return false;
    }
    status = lua_getglobal(m_lua.State(), "GetCameraUp");
    if (status == LUA_TNIL)
    {
        BB_WARNING(false, "lua couldn't get global function GetCameraUp", WarningType::HIGH);
        return false;
    }
    status = lua_getglobal(m_lua.State(), "GetCameraForward");
    if (status == LUA_TNIL)
    {
        BB_WARNING(false, "lua couldn't get global function GetCameraForward", WarningType::HIGH);
        return false;
    }
    return true;
}

static void LoadECSFunction(lua_State* a_state, const lua_CFunction a_function, const char* a_func_name)
{
    lua_pushvalue(a_state, -1);
    lua_pushcclosure(a_state, a_function, 1);
    lua_setglobal(a_state, a_func_name);
}

#define LUA_FUNC_NAME(func) luaapi::func, #func

void GameInstance::RegisterLuaCFunctions()
{
    LuaStackScope scope(m_lua.State());
    lua_registerbbtypes(m_lua.State());

    lua_pushlightuserdata(m_lua.State(), this);

    LoadECSFunction(m_lua.State(), LUA_FUNC_NAME(ECSCreateEntity));
    LoadECSFunction(m_lua.State(), LUA_FUNC_NAME(ECSGetPosition));
    LoadECSFunction(m_lua.State(), LUA_FUNC_NAME(ECSSetPosition));
    LoadECSFunction(m_lua.State(), LUA_FUNC_NAME(ECSTranslate));

    LoadECSFunction(m_lua.State(), LUA_FUNC_NAME(CreateEntityFromJson));

    LoadECSFunction(m_lua.State(), LUA_FUNC_NAME(InputActionIsPressed));
    LoadECSFunction(m_lua.State(), LUA_FUNC_NAME(InputActionIsHeld));
    LoadECSFunction(m_lua.State(), LUA_FUNC_NAME(InputActionIsReleased));
    LoadECSFunction(m_lua.State(), LUA_FUNC_NAME(InputActionGetFloat));
    LoadECSFunction(m_lua.State(), LUA_FUNC_NAME(InputActionGetFloat2));
}
