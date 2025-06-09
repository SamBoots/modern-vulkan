#include "LuaEngine.hpp"
#include "lualib.h"
#include "lauxlib.h"
#include "Logger.h"
#include "lua.hpp"

using namespace BB;

const char* lua_glob = " \
message = \"Hello from Lua!\" \
player_health = 100\
is_game_running = true\
pi_value = 3.14159\
\
print(\"Lua script executed successfully\")";

static void* LuaAlloc(void* a_user_data, void* a_ptr, const size_t a_old_size, const size_t a_new_size)
{
    FreelistInterface* allocator = reinterpret_cast<FreelistInterface*>(a_user_data);

    if (a_new_size == 0)
    {
        allocator->Free(a_ptr);
        return nullptr;
    }
    else
        return allocator->Alloc(a_new_size, 8);
}

bool LuaContext::Init(MemoryArena& a_arena, const size_t a_lua_mem_size)
{
    if (m_state == nullptr)
        return false;
    m_allocator.Initialize(a_arena, a_lua_mem_size);
    m_state = lua_newstate(LuaAlloc, &m_allocator);

    luaL_dostring(m_state, lua_glob);

    lua_getglobal(m_state, "player_health");
    lua_getglobal(m_state, "message");
    const char* str = lua_tostring(m_state, -1);
    const int player_health = lua_tointeger(m_state, -2);
    BB_WARNING(str, str, WarningType::INFO);

    return true;
}

void LuaContext::Update(const float a_delta_time)
{
    
}

int LuaContext::GetStackTopIndex() const
{
    return lua_gettop(m_state);
}
