#pragma once
#include "Common.h"

struct lua_State;

namespace BB
{
    class LuaStackScope
    {
    public:
        LuaStackScope(lua_State*& a_state);
        ~LuaStackScope();

    private:
        lua_State*& m_state;
        uint32_t m_stack_index;
    };

    void lua_RegisterBBTypes(lua_State* a_state);

    bool lua_isfloat3(lua_State* a_state, int a_index);
    void lua_pushfloat3(lua_State* a_state, const float3 a_float3);
    float3* lua_getfloat3(lua_State* a_state, const int a_index);
}