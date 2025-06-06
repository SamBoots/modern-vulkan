#include "LuaEngine.hpp"
#include "lualib.h"
#include "lauxlib.h"

using namespace BB;

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

    return true;
}

void LuaContext::Update(const float a_delta_time)
{

}
