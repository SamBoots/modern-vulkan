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
        int m_stack_index;
    };

    void lua_registerbbtypes(lua_State* a_state);

    bool lua_isbbhandle(lua_State* a_state, int a_index);
    void lua_pushbbhandle(lua_State* a_state, const uint64_t a_handle);
    uint64_t* lua_getbbhandle(lua_State* a_state, const int a_index);

    bool lua_isfloat3(lua_State* a_state, int a_index);
    void lua_pushfloat3(lua_State* a_state, const float3 a_float3);
    float3* lua_getfloat3(lua_State* a_state, const int a_index);
}
