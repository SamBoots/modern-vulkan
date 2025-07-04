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

int luaapi::CreateEntityFromJson(lua_State* a_state)
{
    const char* json_name = lua_tostring(a_state, 1);
    GameInstance* inst = GetGameInstance(a_state);
    PathString json_path = inst->GetProjectPath();
    json_path.AddPathNoSlash(json_name);
    const ECSEntity entity = inst->GetSceneHierarchy().CreateEntityFromJson(inst->GetMemory(), json_path);
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
