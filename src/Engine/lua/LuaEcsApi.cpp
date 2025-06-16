#include "LuaECSApi.hpp"
#include "LuaTypes.hpp"
#include "lualib.h"
#include "lauxlib.h"

#include "HID.h"

#include "ecs/EntityComponentSystem.hpp"
#include "InputSystem.hpp"

using namespace BB;

static EntityComponentSystem* GetECS(lua_State* m_state)
{
    return reinterpret_cast<EntityComponentSystem*>(lua_touserdata(m_state, lua_upvalueindex(1)));
}

int luaapi::ECSCreateEntity(lua_State* m_state)
{
    const NameComponent name = lua_tostring(m_state, 1);
    const ECSEntity parent = ECSEntity(*lua_getbbhandle(m_state, 2));
    const float3 pos = *lua_getfloat3(m_state, 3);

    EntityComponentSystem* ecs = GetECS(m_state);
    const ECSEntity entity = ecs->CreateEntity(name, parent, float3(pos));

    lua_pushbbhandle(m_state, entity.handle);
    return 1;
}

int luaapi::ECSGetPosition(lua_State* m_state)
{
    const ECSEntity entity = ECSEntity(*lua_getbbhandle(m_state, 1));
    EntityComponentSystem* ecs = GetECS(m_state);
            
    const float3 pos = ecs->GetPosition(entity);

    lua_pushfloat3(m_state, pos);
    return 1;
}

int luaapi::ECSSetPosition(lua_State* m_state)
{
    const ECSEntity entity = ECSEntity(*lua_getbbhandle(m_state, 1));
    const float3 pos = *lua_getfloat3(m_state, 2);

    EntityComponentSystem* ecs = GetECS(m_state);
    ecs->SetPosition(entity, pos);

    return 0;
}

int luaapi::ECSTranslate(lua_State* m_state)
{
    const ECSEntity entity = ECSEntity(*lua_getbbhandle(m_state, 1));
    const float3 move = *lua_getfloat3(m_state, 2);

    EntityComponentSystem* ecs = GetECS(m_state);
    ecs->Translate(entity, move);

    return 0;
}

static InputActionHandle LuaLoadInputHandle(lua_State* m_state, const int a_index)
{
    uint64_t* handle = lua_getbbhandle(m_state, a_index);

    if (handle)
        return InputActionHandle(*handle);

    return InputActionHandle();
}

int luaapi::InputActionIsPressed(lua_State* m_state)
{
    const bool res = Input::InputActionIsPressed(LuaLoadInputHandle(m_state, 1));
    lua_pushboolean(m_state, res);
    return 1;
}

int luaapi::InputActionIsHeld(lua_State* m_state)
{
    const bool res = Input::InputActionIsHeld(LuaLoadInputHandle(m_state, 1));
    lua_pushboolean(m_state, res);
    return 1;
}

int luaapi::InputActionIsReleased(lua_State* m_state)
{
    const bool res = Input::InputActionIsReleased(LuaLoadInputHandle(m_state, 1));
    lua_pushboolean(m_state, res);
    return 1;
}

int luaapi::InputActionGetFloat(lua_State* m_state)
{
    const float res = Input::InputActionGetFloat(LuaLoadInputHandle(m_state, 1));
    lua_pushnumber(m_state, res);
    return 1;
}

int luaapi::InputActionGetFloat2(lua_State* m_state)
{
    const float2 res = Input::InputActionGetFloat2(LuaLoadInputHandle(m_state, 1));
    lua_pushnumber(m_state, res.x);
    lua_pushnumber(m_state, res.y);
    return 2;
}
