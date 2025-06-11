#include "LuaEngine.hpp"
#include "LuaEcsApi.inl"

#include "Logger.h"

using namespace BB;

static const char* lua_glob = 
R"(
function calculate_damage(base_damage, multiplier)
    local result = base_damage * multiplier
    return result
end

function spawn_entity_and_move(start_pos_x, start_pos_y, start_pos_z, move_pos_x, move_pos_y, move_pos_z)
    local entity = ECSCreateEntity("entity", 0, start_pos_x, start_pos_y, start_pos_z)
    ECSTranslate(entity, move_pos_x, move_pos_y, move_pos_z);
    return ECSGetPosition(entity);
end
)";

class LuaStackScope
{
public:
    LuaStackScope(lua_State*& a_state) : m_state(a_state) { m_stack_index = lua_gettop(m_state); }
    ~LuaStackScope() { lua_settop(m_state, m_stack_index); }

private:
    lua_State*& m_state;
    uint32_t m_stack_index;
};

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

    return true;
}

int LuaContext::GetStackTopIndex() const
{
    return lua_gettop(m_state);
}

bool LuaECSEngine::Init(MemoryArena& a_arena, EntityComponentSystem* a_psystem, const size_t a_lua_mem_size)
{
    m_context.Init(a_arena, a_lua_mem_size);
    LuaStackScope scope(m_context.GetState());
    LoadECSFunctions(a_psystem);

    lua_getglobal(m_context.GetState(), "spawn_entity_and_move");
    lua_pushnumber(m_context.GetState(), 1.0);
    lua_pushnumber(m_context.GetState(), 2.0);
    lua_pushnumber(m_context.GetState(), 3.0);
    lua_pushnumber(m_context.GetState(), 10.0);
    lua_pushnumber(m_context.GetState(), 20.0);
    lua_pushnumber(m_context.GetState(), 30.0);
    int call_result = lua_pcall(m_context.GetState(), 6, 3, 0);

    float move_x = luaapi::luaGetFloat(m_context.GetState(), -3);
    float move_y = luaapi::luaGetFloat(m_context.GetState(), -2);
    float move_z = luaapi::luaGetFloat(m_context.GetState(), -1);

    return true;
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

    LoadECSFunction(luaapi::ECSCreateEntity, "ECSCreateEntity");
    LoadECSFunction(luaapi::ECSGetPosition, "ECSGetPosition");
    LoadECSFunction(luaapi::ECSSetPosition, "ECSSetPosition");
    LoadECSFunction(luaapi::ECSTranslate, "ECSTranslate");
}

void LuaECSEngine::LoadECSFunction(const lua_CFunction a_function, const char* a_func_name)
{
    lua_pushvalue(m_context.GetState(), -1);
    lua_pushcclosure(m_context.GetState(), a_function, 1);
    lua_setglobal(m_context.GetState(), a_func_name);
}
