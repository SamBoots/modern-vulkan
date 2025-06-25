#pragma once
#include "Enginefwd.hpp"
#include "Allocators/MemoryInterfaces.hpp"
#include "Utils/Slice.h"
#include "lua.h"

#include "Storage/BBString.h"

namespace BB
{
    class LuaContext
    {
    public:
        bool Init(MemoryArena& a_arena, const size_t a_lua_mem_size);
        bool LoadLuaFile(const StringView& a_file_path);
        bool LoadLuaDirectory(MemoryArena& a_temp_arena, const StringView& a_file_path);
        bool RegisterActionHandlesLua(const InputChannelHandle a_channel);

        lua_State*& State() { return m_state; }

    private:
        lua_State* m_state = nullptr;
        FreelistInterface m_allocator;
    };
}
