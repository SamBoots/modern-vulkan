#include "LuaEngine.hpp"
#include "LuaECSApi.hpp"

#include "LuaTypes.hpp"
#include "lauxlib.h"
#include "lualib.h"
#include "Logger.h"

using namespace BB;


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
    lua_registerbbtypes(m_state);

    return true;
}

bool LuaECSEngine::Init(MemoryArena& a_arena, EntityComponentSystem* a_psystem, const size_t a_lua_mem_size)
{
    m_context.Init(a_arena, a_lua_mem_size);

    LuaStackScope scope(m_context.GetState());
    LoadECSFunctions(a_psystem);
    LoadInputFunctions();

    return true;
}

#define LUA_FUNC_NAME(func) luaapi::func, #func

void LuaECSEngine::LoadECSFunctions(EntityComponentSystem* a_psystem)
{
    lua_pushlightuserdata(m_context.GetState(), a_psystem);

    LoadECSFunction(LUA_FUNC_NAME(ECSCreateEntity));
    LoadECSFunction(LUA_FUNC_NAME(ECSGetPosition));
    LoadECSFunction(LUA_FUNC_NAME(ECSSetPosition));
    LoadECSFunction(LUA_FUNC_NAME(ECSTranslate));
}

void LuaECSEngine::LoadInputFunctions()
{
    LoadInputFunction(LUA_FUNC_NAME(InputActionIsPressed));
    LoadInputFunction(LUA_FUNC_NAME(InputActionIsHeld));
    LoadInputFunction(LUA_FUNC_NAME(InputActionIsReleased));
    LoadInputFunction(LUA_FUNC_NAME(InputActionGetFloat));
    LoadInputFunction(LUA_FUNC_NAME(InputActionGetFloat2));
}

void LuaECSEngine::LoadECSFunction(const lua_CFunction a_function, const char* a_func_name)
{
    lua_pushvalue(m_context.GetState(), -1);
    lua_pushcclosure(m_context.GetState(), a_function, 1);
    lua_setglobal(m_context.GetState(), a_func_name);
}

void LuaECSEngine::LoadInputFunction(const lua_CFunction a_function, const char* a_func_name)
{
    lua_pushcfunction(m_context.GetState(), a_function);
    lua_setglobal(m_context.GetState(), a_func_name);
}
