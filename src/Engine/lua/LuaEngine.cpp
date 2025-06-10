#include "LuaEngine.hpp"
#include "LuaEcsApi.inl"

#include "Logger.h"

using namespace BB;

static const char* lua_glob = 
R"(
message = "Hello from Lua!";
player_health = 100;
is_game_running = true;
pi_value = 3.14159;
)";

static const char* lua_func =
R"(
function calculate_damage(base_damage, multiplier)
    local result = base_damage * multiplier
    return result
end
)";

#define LUA_FUNC_SET(name) BB_CONCAT(luaapi::, name), #name


static void* LuaAlloc(void* a_user_data, void* a_ptr, const size_t a_old_size, const size_t a_new_size)
{
    FreelistInterface* allocator = reinterpret_cast<FreelistInterface*>(a_user_data);

    if (a_ptr && a_new_size == 0)
    {
        allocator->Free(a_ptr);
    }
    else if (a_new_size)
    {
        void* ptr = allocator->Alloc(a_new_size, 8);
        BB_ASSERT(ptr, "failed to allocate lua mem");
        return ptr;
    }
    return nullptr;
}

bool LuaContext::Init(MemoryArena& a_arena, const size_t a_lua_mem_size)
{
    if (m_state != nullptr)
        return false;
    m_allocator.Initialize(a_arena, a_lua_mem_size);
    m_state = luaL_newstate();
    luaL_openlibs(m_state);
    int result = luaL_dostring(m_state, lua_glob);

    lua_getglobal(m_state, "player_health");
    lua_getglobal(m_state, "message");
    const char* str = lua_tostring(m_state, -1);
    const lua_Integer player_health = lua_tointeger(m_state, -2);
    BB_WARNING(str, str, WarningType::INFO);

    luaL_dostring(m_state, lua_func);

    return true;
}

int LuaContext::GetStackTopIndex() const
{
    return lua_gettop(m_state);
}

bool LuaECSEngine::Init(MemoryArena& a_arena, EntityComponentSystem* a_psystem, const size_t a_lua_mem_size)
{
    m_context.Init(a_arena, a_lua_mem_size);
    LoadECSFunctions(a_psystem);
}

void LuaECSEngine::Update(const float a_delta_time)
{
    lua_getglobal(m_context.GetState(), "calculate_damage");

    lua_pushnumber(m_context.GetState(), 50.0);
    lua_pushnumber(m_context.GetState(), 1.5);

    int call_result = lua_pcall(m_context.GetState(), 2, 1, 0);

    double damage = lua_tonumber(m_context.GetState(), -1);
}

void LuaECSEngine::LoadECSFunctions(EntityComponentSystem* a_psystem)
{
    lua_pushlightuserdata(m_context.GetState(), a_psystem);

    LoadECSFunction(LUA_FUNC_SET(CreateECSObject));
}

void LuaECSEngine::LoadECSFunction(const lua_CFunction a_function, const char* a_func_name)
{
    lua_pushvalue(m_context.GetState(), -1);  // Duplicate the ECS pointer
    lua_pushcclosure(m_context.GetState(), a_function, 1);
    lua_setglobal(m_context.GetState(), a_func_name);
}
