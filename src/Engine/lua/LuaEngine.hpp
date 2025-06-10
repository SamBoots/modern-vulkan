#pragma once
#include "Enginefwd.hpp"
#include "Allocators/MemoryInterfaces.hpp"
#include "lua.h"

namespace BB
{
    class LuaContext
    {
    public:
        bool Init(MemoryArena& a_arena, const size_t a_lua_mem_size);

        void Update(const float a_delta_time);

        int GetStackTopIndex() const;
        lua_State* GetState() const { return m_state; }

    private:
        lua_State* m_state = nullptr;
        FreelistInterface m_allocator;
    };

    class LuaECSEngine
    {
    public:
        bool Init(MemoryArena& a_arena, class EntityComponentSystem* a_psystem, const size_t a_lua_mem_size);

        void Update(const float a_delta_time);

    private:
        void LoadECSFunctions(class EntityComponentSystem* a_psystem);
        void LoadECSFunction(const lua_CFunction a_function, const char* a_func_name);

        LuaContext m_context;

    };
}
