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

    private:
        lua_State* m_state = nullptr;
        FreelistInterface m_allocator;
    };
}
