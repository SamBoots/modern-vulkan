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

function spawn_entity_and_move(a_start_pos, a_move)
    local entity = ECSCreateEntity("entity", 0, a_start_pos)
    ECSTranslate(entity, a_move);
    return ECSGetPosition(entity);
end
)";

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
    lua_RegisterBBTypes(m_state);

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
    const float3 pos = float3(1.f, 2.f, 3.f);
    const float3 move = float3(10.f, 20.f, 30.f);
    lua_pushfloat3(m_context.GetState(), pos);
    lua_pushfloat3(m_context.GetState(), move);
    int top0 = m_context.GetStackTopIndex();
    int call_result = lua_pcall(m_context.GetState(), 2, 1, 0);
    int top1 = m_context.GetStackTopIndex();
    const float3 new_pos = *lua_getfloat3(m_context.GetState(), -1);

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
