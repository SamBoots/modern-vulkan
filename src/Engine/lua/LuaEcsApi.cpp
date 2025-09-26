#include "LuaECSApi.hpp"
#include "LuaTypes.hpp"
#include "lualib.h"
#include "lauxlib.h"

#include "HID.h"

#include "GameInstance.hpp"
#include "InputSystem.hpp"

using namespace BB;

static GameInstance* GetGameInstance(lua_State* a_state)
{
    return reinterpret_cast<GameInstance*>(lua_touserdata(a_state, lua_upvalueindex(1)));
}

int luaapi::ECSCreateEntity(lua_State* a_state)
{
    const NameComponent name = lua_tostring(a_state, 1);
    const ECSEntity parent = ECSEntity(*lua_getbbhandle(a_state, 2));
    const float3 pos = *lua_getfloat3(a_state, 3);

    GameInstance* inst = GetGameInstance(a_state);
    const ECSEntity entity = inst->GetSceneHierarchy().GetECS().CreateEntity(name, parent, float3(pos));

    lua_pushbbhandle(a_state, entity.handle);
    return 1;
}

int luaapi::ECSDestroyEntity(lua_State* a_state)
{
    const ECSEntity entity = ECSEntity(*lua_getbbhandle(a_state, 1));
    GameInstance* inst = GetGameInstance(a_state);
    bool success = inst->GetSceneHierarchy().GetECS().DestroyEntity(entity);

    lua_pushboolean(a_state, success);
    return 1;
}

int luaapi::ECSGetPosition(lua_State* a_state)
{
    const ECSEntity entity = ECSEntity(*lua_getbbhandle(a_state, 1));
    GameInstance* inst = GetGameInstance(a_state);
            
    const float3 pos = inst->GetSceneHierarchy().GetECS().GetPosition(entity);

    lua_pushfloat3(a_state, pos);
    return 1;
}

int luaapi::ECSSetPosition(lua_State* a_state)
{
    const ECSEntity entity = ECSEntity(*lua_getbbhandle(a_state, 1));
    const float3 pos = *lua_getfloat3(a_state, 2);

    GameInstance* inst = GetGameInstance(a_state);
    inst->GetSceneHierarchy().GetECS().SetPosition(entity, pos);

    return 0;
}

int luaapi::ECSTranslate(lua_State* a_state)
{
    const ECSEntity entity = ECSEntity(*lua_getbbhandle(a_state, 1));
    const float3 move = *lua_getfloat3(a_state, 2);

    GameInstance* inst = GetGameInstance(a_state);
    inst->GetSceneHierarchy().GetECS().Translate(entity, move);

    return 0;
}

int luaapi::GetScreenResolution(lua_State* a_state)
{
    GameInstance* inst = GetGameInstance(a_state);
    const uint2 extent = inst->GetViewport().GetExtent();
    lua_pushinteger(a_state, static_cast<int>(extent.x));
    lua_pushinteger(a_state, static_cast<int>(extent.y));
    return 2;
}

int luaapi::CreateEntityFromJson(lua_State* a_state)
{
    const char* json_name = lua_tostring(a_state, 1);
    GameInstance* inst = GetGameInstance(a_state);
    PathString json_path = inst->GetProjectPath();
    json_path.AddPathNoSlash(json_name);
    const ECSEntity entity = inst->GetSceneHierarchy().CreateEntityFromJson(inst->GetMemory(), json_path, inst->GetViewport().GetExtent());
    lua_pushbbhandle(a_state, entity.handle);
    return 1;
}

static InputActionHandle LuaLoadInputHandle(lua_State* a_state, const int a_index)
{
    uint64_t* handle = lua_getbbhandle(a_state, a_index);

    if (handle)
        return InputActionHandle(*handle);

    return InputActionHandle();
}

int luaapi::InputActionIsPressed(lua_State* a_state)
{
    const bool res = Input::InputActionIsPressed(GetGameInstance(a_state)->GetInputChannel(), LuaLoadInputHandle(a_state, 1));
    lua_pushboolean(a_state, res);
    return 1;
}

int luaapi::InputActionIsHeld(lua_State* a_state)
{
    const bool res = Input::InputActionIsHeld(GetGameInstance(a_state)->GetInputChannel(), LuaLoadInputHandle(a_state, 1));
    lua_pushboolean(a_state, res);
    return 1;
}

int luaapi::InputActionIsReleased(lua_State* a_state)
{
    const bool res = Input::InputActionIsReleased(GetGameInstance(a_state)->GetInputChannel(), LuaLoadInputHandle(a_state, 1));
    lua_pushboolean(a_state, res);
    return 1;
}

int luaapi::InputActionGetFloat(lua_State* a_state)
{
    const float res = Input::InputActionGetFloat(GetGameInstance(a_state)->GetInputChannel(), LuaLoadInputHandle(a_state, 1));
    lua_pushnumber(a_state, res);
    return 1;
}

int luaapi::InputActionGetFloat2(lua_State* a_state)
{
    const float2 res = Input::InputActionGetFloat2(GetGameInstance(a_state)->GetInputChannel(), LuaLoadInputHandle(a_state, 1));
    lua_pushnumber(a_state, res.x);
    lua_pushnumber(a_state, res.y);
    return 2;
}

int luaapi::UICreatePanel(lua_State* a_state)
{
    const float pos_x = lua_tonumber(a_state, 1);
    const float pos_y = lua_tonumber(a_state, 2);
    const float extent_x = lua_tonumber(a_state, 3);
    const float extent_y = lua_tonumber(a_state, 4);
    const int r = lua_tointeger(a_state, 5);
    const int g = lua_tointeger(a_state, 6);
    const int b = lua_tointeger(a_state, 7);
    const int a = lua_tointeger(a_state, 8);

    GameInstance* inst = GetGameInstance(a_state);
    inst->GetSceneHierarchy().GetECS().GetRenderSystem().GetUIStage().CreatePanel(
        float2(pos_x, pos_y), 
        float2(extent_x, extent_y), 
        Color(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b), static_cast<uint8_t>(a)));
    return 0;
}

int luaapi::UICreateText(lua_State* a_state)
{
    const float pos_x = lua_tonumber(a_state, 1);
    const float pos_y = lua_tonumber(a_state, 2);
    const float extent_x = lua_tonumber(a_state, 3);
    const float extent_y = lua_tonumber(a_state, 4);
    const int r = lua_tointeger(a_state, 5);
    const int g = lua_tointeger(a_state, 6);
    const int b = lua_tointeger(a_state, 7);
    const int a = lua_tointeger(a_state, 8);
    const float spacing = lua_tonumber(a_state, 9);
    const float max_x = lua_tonumber(a_state, 10);
    const char* str = lua_tostring(a_state, 11);

    GameInstance* inst = GetGameInstance(a_state);
    inst->GetSceneHierarchy().GetECS().GetRenderSystem().GetUIStage().CreateText(
        float2(pos_x, pos_y), 
        float2(extent_x, extent_y), 
        Color(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b), static_cast<uint8_t>(a)),
        str, max_x, spacing,
        inst->GetSceneHierarchy().GetECS().GetRenderSystem().GetDefaultFont());
    return 0;
}
