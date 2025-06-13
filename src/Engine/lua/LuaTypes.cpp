#include "LuaTypes.hpp"
#include "lua.h"

#include "Math/Math.inl"

using namespace BB;

LuaStackScope::LuaStackScope(lua_State*& a_state) : m_state(a_state) 
{ 
    m_stack_index = lua_gettop(m_state); 
}

LuaStackScope::~LuaStackScope() 
{ 
    lua_settop(m_state, m_stack_index); 
}

static const char* FLOAT3_LUA_NAME = "float3";

static void RegisterFloat3(lua_State* a_state)
{
    LuaStackScope stack_scope();
    lua_newtable(a_state);

    lua_createtable(a_state, 0, 3);
    int metatable = lua_gettop(a_state);

    lua_pushvalue(a_state, metatable);
    lua_setfield(a_state, LUA_REGISTRYINDEX, FLOAT3_LUA_NAME);

    static auto lua_newindex = [](lua_State* a_state) -> int
        {
            float3* floats = lua_getfloat3(a_state, 1);
            if (floats && !lua_isstring(a_state, 2) && !lua_isnumber(a_state, 3))
                return 0;

            const char* key = lua_tostring(a_state, 2);
            const float value = static_cast<float>(lua_tonumber(a_state, 3));

            if (strcmp(key, "x") == 0)
                floats->x = value;
            else if (strcmp(key, "y") == 0)
                floats->y = value;
            else if (strcmp(key, "z") == 0)
                floats->z = value;

            return 0;
        };
    static auto lua_index = [](lua_State* a_state) -> int
        {
            float3* floats = lua_getfloat3(a_state, 1);
            if (!floats && !lua_isstring(a_state, 2))
            {
                lua_pushnil(a_state);
                return 1;
            }

            const char* key = lua_tostring(a_state, 2);

            if (strcmp(key, "x") == 0)
                lua_pushnumber(a_state, floats->x);
            else if (strcmp(key, "y") == 0)
                lua_pushnumber(a_state, floats->y);
            else if (strcmp(key, "z") == 0)
                lua_pushnumber(a_state, floats->z);
            else
                lua_pushnil(a_state);

            return 1;
        };
    static auto lua_new = [](lua_State* a_state) -> int
        {
            float3 dat = float3(0.f);
            if (lua_gettop(a_state) >= 1 && lua_isnumber(a_state, 1)) {
                dat = float3(static_cast<float>(lua_tonumber(a_state, 1)));
            }
            if (lua_gettop(a_state) >= 2 && lua_isnumber(a_state, 2)) {
                dat.y = static_cast<float>(lua_tonumber(a_state, 2));
            }
            if (lua_gettop(a_state) >= 3 && lua_isnumber(a_state, 3)) {
                dat.z = static_cast<float>(lua_tonumber(a_state, 3));
            }

            lua_pushfloat3(a_state, dat);
            return 1;
        };
    static auto lua_add = [](lua_State* a_state) -> int
        {
            float3* floats = lua_getfloat3(a_state, 1);
            if (lua_isnumber(a_state, 2))
                lua_pushfloat3(a_state, *floats + static_cast<float>(lua_tonumber(a_state, 2)));
            else
            {
                float3* value = lua_getfloat3(a_state, 2);
                if (value)
                    lua_pushfloat3(a_state, *floats + *value);
                else
                    lua_pushnil(a_state);
            }
            return 1;
        };
    static auto lua_sub = [](lua_State* a_state) -> int
        {
            float3* floats = lua_getfloat3(a_state, 1);
            if (lua_isnumber(a_state, 2))
                lua_pushfloat3(a_state, *floats - static_cast<float>(lua_tonumber(a_state, 2)));
            else
            {
                float3* value = lua_getfloat3(a_state, 2);
                if (value)
                    lua_pushfloat3(a_state, *floats - *value);
                else
                    lua_pushnil(a_state);
            }
            return 1;
        };
    static auto lua_mul = [](lua_State* a_state) -> int
        {
            float3* floats = lua_getfloat3(a_state, 1);
            if (lua_isnumber(a_state, 2))
                lua_pushfloat3(a_state, *floats * static_cast<float>(lua_tonumber(a_state, 2)));
            else
            {
                float3* value = lua_getfloat3(a_state, 2);
                if (value)
                    lua_pushfloat3(a_state, *floats * *value);
                else
                    lua_pushnil(a_state);
            }
            return 1;
        };
    static auto lua_normalize = [](lua_State* a_state) -> int
        {
            float3* floats = lua_getfloat3(a_state, 1);
            lua_pushfloat3(a_state, Float3Normalize(*floats));
            return 1;
        };
    static auto lua_cross = [](lua_State* a_state) -> int
        {
            float3* floats0 = lua_getfloat3(a_state, 1);
            float3* floats1 = lua_getfloat3(a_state, 2);
            lua_pushfloat3(a_state, Float3Cross(*floats0, *floats1));
            return 1;
        };

    lua_pushcfunction(a_state, lua_index);
    lua_setfield(a_state, metatable, "__index");

    lua_pushcfunction(a_state, lua_newindex);
    lua_setfield(a_state, metatable, "__newindex");

    lua_pushcfunction(a_state, lua_add);
    lua_setfield(a_state, metatable, "__add");

    lua_pushcfunction(a_state, lua_sub);
    lua_setfield(a_state, metatable, "__sub");

    lua_pushcfunction(a_state, lua_mul);
    lua_setfield(a_state, metatable, "__mul");

    lua_pushcfunction(a_state, lua_normalize);
    lua_setfield(a_state, metatable, "Normalize");

    lua_pushcfunction(a_state, lua_cross);
    lua_setfield(a_state, metatable, "Cross");

    lua_pushcfunction(a_state, lua_new);

    lua_setglobal(a_state, FLOAT3_LUA_NAME);
}

void BB::lua_registerbbtypes(lua_State* a_state)
{
    RegisterFloat3(a_state);
}

bool BB::lua_isfloat3(lua_State* a_state, int a_index)
{
    int top0 = lua_gettop(a_state);
    if (!lua_isuserdata(a_state, a_index))
        return false;

    if (!lua_getmetatable(a_state, a_index))
        return false;

    lua_getfield(a_state, LUA_REGISTRYINDEX, FLOAT3_LUA_NAME);

    bool result = lua_rawequal(a_state, -1, -2);

    lua_pop(a_state, 2);

    return result;
}

void BB::lua_pushfloat3(lua_State* a_state, const float3 a_float3)
{
    float3* userdata = static_cast<float3*>(lua_newuserdata(a_state, sizeof(float3)));
    *userdata = a_float3;
    lua_getfield(a_state, LUA_REGISTRYINDEX, FLOAT3_LUA_NAME);
    lua_setmetatable(a_state, -2);
}

float3* BB::lua_getfloat3(lua_State* a_state, const int a_index)
{
    if (!lua_isfloat3(a_state, a_index))
    {
        return nullptr;
    }
    return static_cast<float3*>(lua_touserdata(a_state, a_index));
}
