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

constexpr const char FLOAT3_LUA_NAME[] = "float3";
constexpr const char BBHANDLE_LUA_NAME[] = "bbhandle";

static void RegisterFloat3(lua_State* a_state)
{
    LuaStackScope stack_scope(a_state);
    lua_newtable(a_state);

    lua_createtable(a_state, 0, 3);
    int metatable = lua_gettop(a_state);

    lua_pushvalue(a_state, metatable);
    lua_setfield(a_state, LUA_REGISTRYINDEX, FLOAT3_LUA_NAME);

    static auto lua_newindex = [](lua_State* a_pstate) -> int
        {
            float3* floats = lua_getfloat3(a_pstate, 1);
            if (floats && !lua_isstring(a_pstate, 2) && !lua_isnumber(a_pstate, 3))
                return 0;

            const char* key = lua_tostring(a_pstate, 2);
            const float value = lua_tonumber(a_pstate, 3);

            if (strcmp(key, "x") == 0)
                floats->x = value;
            else if (strcmp(key, "y") == 0)
                floats->y = value;
            else if (strcmp(key, "z") == 0)
                floats->z = value;

            return 0;
        };
    static auto lua_index = [](lua_State* a_pstate) -> int
        {
            float3* floats = lua_getfloat3(a_pstate, 1);
            if (!floats && !lua_isstring(a_pstate, 2))
            {
                lua_pushnil(a_pstate);
                return 1;
            }

            const char* key = lua_tostring(a_pstate, 2);

            if (strcmp(key, "x") == 0)
                lua_pushnumber(a_pstate, floats->x);
            else if (strcmp(key, "y") == 0)
                lua_pushnumber(a_pstate, floats->y);
            else if (strcmp(key, "z") == 0)
                lua_pushnumber(a_pstate, floats->z);
            else
                lua_pushnil(a_pstate);

            return 1;
        };
    static auto lua_new = [](lua_State* a_pstate) -> int
        {
            float3 dat = float3(0.f);
            if (lua_gettop(a_pstate) >= 1 && lua_isnumber(a_pstate, 1)) {
                dat = float3(lua_tonumber(a_pstate, 1));
            }
            if (lua_gettop(a_pstate) >= 2 && lua_isnumber(a_pstate, 2)) {
                dat.y = lua_tonumber(a_pstate, 2);
            }
            if (lua_gettop(a_pstate) >= 3 && lua_isnumber(a_pstate, 3)) {
                dat.z = lua_tonumber(a_pstate, 3);
            }

            lua_pushfloat3(a_pstate, dat);
            return 1;
        };
    static auto lua_add = [](lua_State* a_pstate) -> int
        {
            float3* floats = lua_getfloat3(a_pstate, 1);
            if (lua_isnumber(a_pstate, 2))
                lua_pushfloat3(a_pstate, *floats + lua_tonumber(a_pstate, 2));
            else
            {
                float3* value = lua_getfloat3(a_pstate, 2);
                if (value)
                    lua_pushfloat3(a_pstate, *floats + *value);
                else
                    lua_pushnil(a_pstate);
            }
            return 1;
        };
    static auto lua_sub = [](lua_State* a_pstate) -> int
        {
            float3* floats = lua_getfloat3(a_pstate, 1);
            if (lua_isnumber(a_pstate, 2))
                lua_pushfloat3(a_pstate, *floats - lua_tonumber(a_pstate, 2));
            else
            {
                float3* value = lua_getfloat3(a_pstate, 2);
                if (value)
                    lua_pushfloat3(a_pstate, *floats - *value);
                else
                    lua_pushnil(a_pstate);
            }
            return 1;
        };
    static auto lua_mul = [](lua_State* a_pstate) -> int
        {
            float3* floats = lua_getfloat3(a_pstate, 1);
            if (lua_isnumber(a_pstate, 2))
                lua_pushfloat3(a_pstate, *floats * lua_tonumber(a_pstate, 2));
            else
            {
                float3* value = lua_getfloat3(a_pstate, 2);
                if (value)
                    lua_pushfloat3(a_pstate, *floats * *value);
                else
                    lua_pushnil(a_pstate);
            }
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

    lua_pushcfunction(a_state, lua_new);

    lua_setglobal(a_state, FLOAT3_LUA_NAME);
}

static void RegisterBBHandle(lua_State* a_state)
{
    LuaStackScope stack_scope(a_state);
    lua_newtable(a_state);

    lua_createtable(a_state, 0, 1);
    int metatable = lua_gettop(a_state);

    lua_pushvalue(a_state, metatable);
    lua_setfield(a_state, LUA_REGISTRYINDEX, BBHANDLE_LUA_NAME);

    static auto lua_new = [](lua_State* a_pstate) -> int
        {
            const uint64_t handle = reinterpret_cast<const uint64_t>(lua_touserdata(a_pstate, 1));
            lua_pushbbhandle(a_pstate, handle);
            return 1;
        };

    lua_pushcfunction(a_state, lua_new);

    lua_setglobal(a_state, BBHANDLE_LUA_NAME);
}

static int lua_float3Cross(lua_State* a_state)
{
    float3* floats0 = lua_getfloat3(a_state, 1);
    float3* floats1 = lua_getfloat3(a_state, 2);
    const float3 cross = Float3Cross(*floats0, *floats1);
    lua_pushfloat3(a_state, cross);
    return 1;
}

static int lua_float3Normalize(lua_State* a_state)
{
    float3* floats0 = lua_getfloat3(a_state, 1);
    const float3 norm = Float3Normalize(*floats0);
    lua_pushfloat3(a_state, norm);
    return 1;
}

static int lua_float3Rotate(lua_State* a_state)
{
    float3* floats0 = lua_getfloat3(a_state, 1);
    float3* floats1 = lua_getfloat3(a_state, 2);
    const float3 rotated = Float3x3FromRotation(*floats0) * *floats1;
    lua_pushfloat3(a_state, rotated);
    return 1;
}

void BB::lua_registerbbtypes(lua_State* a_state)
{
    LuaStackScope scope(a_state);
    RegisterFloat3(a_state);
    RegisterBBHandle(a_state);

    // register float3 math
    lua_pushcfunction(a_state, lua_float3Normalize);
    lua_setglobal(a_state, "float3Normalize");
    lua_pushcfunction(a_state, lua_float3Cross);
    lua_setglobal(a_state, "float3Cross");
    lua_pushcfunction(a_state, lua_float3Rotate);
    lua_setglobal(a_state, "float3Rotate");
}

template<typename T, const char* NAME>
static bool lua_isTValue(lua_State* a_state, int a_index)
{
    if (!lua_isuserdata(a_state, a_index))
        return false;

    if (!lua_getmetatable(a_state, a_index))
        return false;

    lua_getfield(a_state, LUA_REGISTRYINDEX, NAME);

    bool result = lua_rawequal(a_state, -1, -2);

    lua_pop(a_state, 2);

    return result;
}

template<typename T, const char* NAME>
static void lua_pushTValue(lua_State* a_state, const T a_value)
{
    T* userdata = reinterpret_cast<T*>(lua_newuserdata(a_state, sizeof(T)));
    *userdata = a_value;
    lua_getfield(a_state, LUA_REGISTRYINDEX, NAME);
    lua_setmetatable(a_state, -2);
}

template<typename T, const char* NAME>
static T* lua_getTValue(lua_State* a_state, const int a_index)
{
    if (!lua_isTValue<T, NAME>(a_state, a_index))
    {
        return nullptr;
    }
    return reinterpret_cast<T*>(lua_touserdata(a_state, a_index));
}

bool BB::lua_isbbhandle(lua_State* a_state, int a_index)
{
    return lua_isTValue<uint64_t, BBHANDLE_LUA_NAME>(a_state, a_index);
}

void BB::lua_pushbbhandle(lua_State* a_state, const uint64_t a_handle)
{
    lua_pushTValue<uint64_t, BBHANDLE_LUA_NAME>(a_state, a_handle);
}

uint64_t* BB::lua_getbbhandle(lua_State* a_state, const int a_index)
{
    return lua_getTValue<uint64_t, BBHANDLE_LUA_NAME>(a_state, a_index);
}

bool BB::lua_isfloat3(lua_State* a_state, int a_index)
{
    return lua_isTValue<float3, FLOAT3_LUA_NAME>(a_state, a_index);
}

void BB::lua_pushfloat3(lua_State* a_state, const float3 a_float3)
{
    lua_pushTValue<float3, FLOAT3_LUA_NAME>(a_state, a_float3);
}

float3* BB::lua_getfloat3(lua_State* a_state, const int a_index)
{
    return lua_getTValue<float3, FLOAT3_LUA_NAME>(a_state, a_index);
}
