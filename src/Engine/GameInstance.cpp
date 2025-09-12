#include "GameInstance.hpp"
#include "Engine.hpp"
#include "InputSystem.hpp"

#include "lua/LuaTypes.hpp"
#include "lua/LuaECSApi.hpp"

using namespace BB;

bool GameInstance::Init(const uint2 a_game_viewport_size, const StringView a_project_name, MemoryArena* a_parena, const ConstSlice<PFN_LuaPluginRegisterFunctions> a_register_funcs)
{
    if (a_parena)
        m_arena = *a_parena;
    else
        m_arena = MemoryArenaCreate();

    m_project_name = a_project_name;
    m_project_path = GetRootPath();
    m_project_path.AddPath("projects");
    m_project_path.AddPath(a_project_name);

    {
        PathString input_path = m_project_path;
        input_path.AddPathNoSlash("input.json");
        if (OSFileExist(input_path.c_str()))
            m_input_channel = Input::CreateInputChannelByJson(m_arena, a_project_name, input_path.GetView());
    }

    m_scene_hierarchy.Init(m_arena, STANDARD_ECS_OBJ_COUNT, a_game_viewport_size, a_project_name);

    m_viewport.Init(a_game_viewport_size, int2(0, 0), a_project_name);
    if (a_register_funcs.size())
        m_plugin_functions.Init(m_arena, static_cast<uint32_t>(a_register_funcs.size()));

    for (size_t i = 0; i < a_register_funcs.size(); i++)
        m_plugin_functions.push_back(a_register_funcs[i]);

    m_lua.Init(m_arena, gbSize);

    return InitLua();
}

bool GameInstance::InitLua()
{
    RegisterLuaCFunctions();
    if (m_input_channel.IsValid())
        m_lua.RegisterActionHandlesLua(m_input_channel);

    PathString lua_path = m_project_path;
    lua_path.AddPathNoSlash("lua/");

    PathString lua_include_path = lua_path;
    lua_include_path.AddPathNoSlash("include/?.lua");
    m_lua.AddIncludePath(lua_include_path.GetView());

    MemoryArenaScope(m_arena)
    {
        bool status = m_lua.LoadLuaDirectory(m_arena, lua_path.GetView());
        if (!status)
        {
            SetDirty();
            return false;
        }
    }

    if (!Verify())
        return false;

    for (size_t i = 0; i < m_plugin_functions.size(); i++)
        m_plugin_functions[i](*this);

    m_lua.LoadAndCallFunction("Init", 1);
    const bool init_success = lua_toboolean(m_lua.State(), -1);
    if (init_success)
        return true;

    SetDirty();
    return false;
}


bool GameInstance::Update(const float a_delta_time, const bool a_selected)
{
    if (m_dirty)
        return false;

    const LuaStackScope scope(m_lua.State());

    m_lua.LoadFunction("Update");
    lua_pushnumber(m_lua.State(), static_cast<lua_Number>(a_delta_time));
    lua_pushboolean(m_lua.State(), a_selected);
    m_lua.CallFunction(2, 1);
    const bool update_success = lua_toboolean(m_lua.State(), -1);

    if (!update_success)
    {
        SetDirty();
        return false;
    }

    return true;
}

void GameInstance::Destroy()
{
    m_lua.LoadAndCallFunction("Destroy", 1);
    bool success = lua_isboolean(m_lua.State(), -1);
    BB_WARNING(success, "something went wrong destroying game instance", WarningType::HIGH);
}

float3 GameInstance::GetCameraPos()
{
    if (m_dirty)
        return float3();
    const LuaStackScope scope(m_lua.State());
    m_lua.LoadAndCallFunction("GetCameraPos", 1);
    return *lua_getfloat3(m_lua.State(), -1);
}

float4x4 GameInstance::GetCameraView()
{
    if (m_dirty)
        return float4x4();
    const LuaStackScope scope(m_lua.State());
    const float3 pos = GetCameraPos();
    m_lua.LoadAndCallFunction("GetCameraUp", 1);
    const float3 up = *lua_getfloat3(m_lua.State(), -1);
    m_lua.LoadAndCallFunction("GetCameraForward", 1);
    const float3 forward = *lua_getfloat3(m_lua.State(), -1);

    return Float4x4Lookat(pos, pos + forward, up);
}

lua_State* GameInstance::GetLuaState()
{
    return m_lua.State();
}

bool GameInstance::Verify()
{
    if (m_dirty)
        return false;
    const LuaStackScope scope(m_lua.State());
    if (!m_lua.VerifyGlobal("Init"))
        return false;
    if (!m_lua.VerifyGlobal("Update"))
        return false;
    if (!m_lua.VerifyGlobal("Destroy"))
        return false;
    if (!m_lua.VerifyGlobal("GetCameraPos"))
        return false;
    if (!m_lua.VerifyGlobal("GetCameraUp"))
        return false;
    if (!m_lua.VerifyGlobal("GetCameraForward"))
        return false;
    return true;
}

bool GameInstance::Reload()
{
    Destroy();
    if (!m_lua.Reset())
        return false;
    m_dirty = false;

    return InitLua();
}

bool GameInstance::IsDirty() const
{
    return m_dirty;
}

void GameInstance::SetDirty()
{
    BB_WARNING(false, "Game Instance set to dirty", WarningType::HIGH);
    m_dirty = true;
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
    LoadECSFunction(m_lua.State(), LUA_FUNC_NAME(ECSDestroyEntity));
    LoadECSFunction(m_lua.State(), LUA_FUNC_NAME(ECSGetPosition));
    LoadECSFunction(m_lua.State(), LUA_FUNC_NAME(ECSSetPosition));
    LoadECSFunction(m_lua.State(), LUA_FUNC_NAME(ECSTranslate));

    LoadECSFunction(m_lua.State(), LUA_FUNC_NAME(CreateEntityFromJson));

    LoadECSFunction(m_lua.State(), LUA_FUNC_NAME(InputActionIsPressed));
    LoadECSFunction(m_lua.State(), LUA_FUNC_NAME(InputActionIsHeld));
    LoadECSFunction(m_lua.State(), LUA_FUNC_NAME(InputActionIsReleased));
    LoadECSFunction(m_lua.State(), LUA_FUNC_NAME(InputActionGetFloat));
    LoadECSFunction(m_lua.State(), LUA_FUNC_NAME(InputActionGetFloat2));

    LoadECSFunction(m_lua.State(), LUA_FUNC_NAME(UICreatePanel));
    LoadECSFunction(m_lua.State(), LUA_FUNC_NAME(UICreateText));
}
