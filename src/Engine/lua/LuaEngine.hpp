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

        lua_State*& GetState() { return m_state; }

    private:
        lua_State* m_state = nullptr;
        FreelistInterface m_allocator;
    };

    class LuaECSEngine
    {
    public:
        bool Init(MemoryArena& a_arena, const InputChannelHandle a_channel, class EntityComponentSystem* a_psystem, const size_t a_lua_mem_size);

        lua_State*& GetState() { return m_context.GetState(); }
        bool LoadLuaFile(const StringView& a_file_path);

        bool RegisterActionHandlesLua(const InputChannelHandle a_channel);

    private:
        void LoadECSFunctions(class EntityComponentSystem* a_psystem);
        void LoadInputFunctions(const InputChannelHandle a_channel);
        void LoadECSFunction(const lua_CFunction a_function, const char* a_func_name);
        void LoadInputFunction(const lua_CFunction a_function, const char* a_func_name);

        LuaContext m_context;
    };
}
