#include "LuaEngine.hpp"
#include "LuaECSApi.hpp"

#include "LuaTypes.hpp"
#include "lauxlib.h"
#include "lualib.h"
#include "Logger.h"

#include "InputSystem.hpp"

#include "AssetLoader.hpp"

#include "Program.h"

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

static bool lua_setuppaths(lua_State* a_state)
{
    PathString lua_path = Asset::GetAssetPath();
    lua_path.append("lua\\?.lua");

    StackString<2024> lua_include_paths;

    lua_getglobal(a_state, "package");
    lua_getfield(a_state, -1, "path");
    const char* current_path = lua_tostring(a_state, -1);
    lua_include_paths.append(current_path);
    lua_include_paths.append(";");
    lua_include_paths.append(lua_path);
    lua_pop(a_state, 1);

    lua_pushstring(a_state, lua_include_paths.c_str());
    lua_setfield(a_state, -2, "path");
    return true;
}

bool LuaContext::Init(MemoryArena& a_arena, const size_t a_lua_mem_size)
{
    if (m_state != nullptr)
        return false;
    m_allocator.Initialize(a_arena, a_lua_mem_size);
    m_state = luaL_newstate();

    luaL_openlibs(m_state);

    LuaStackScope scope(m_state);
    lua_registerbbtypes(m_state);
    lua_setuppaths(m_state);

    return true;
}

bool LuaECSEngine::Init(MemoryArena& a_arena, const InputChannelHandle a_channel, EntityComponentSystem* a_psystem, const size_t a_lua_mem_size)
{
    m_context.Init(a_arena, a_lua_mem_size);

    LuaStackScope scope(m_context.GetState());
    LoadECSFunctions(a_psystem);
    LoadInputFunctions(a_channel);

    return true;
}

bool LuaECSEngine::LoadLuaFile(const StringView& a_file_path)
{
    return luaL_dofile(m_context.GetState(), a_file_path.c_str()) == LUA_OK;
}

bool LuaECSEngine::LoadLuaDirectory(MemoryArena& a_temp_arena, const StringView& a_file_path)
{
    ConstSlice<StackString<MAX_PATH_SIZE>> lua_paths;
    PathString path = a_file_path;
    path.append("\\*lua");
    if (!OSGetDirectoryEntries(a_temp_arena, path.c_str(), lua_paths))
        return false;

    for (size_t i = 0; i < lua_paths.size(); i++)
    {
        PathString path = a_file_path;
        path.push_directory_slash();
        path.append(lua_paths[i].GetView());
        bool status = LoadLuaFile(path.GetView());
        BB_ASSERT(status == true, lua_tostring(m_context.GetState(), -1));
    }

    return true;
}

bool LuaECSEngine::RegisterActionHandlesLua(const InputChannelHandle a_channel)
{
    const Slice input_actions = Input::GetAllInputActions(a_channel);
    LuaStackScope scope(m_context.GetState());
    for (size_t i = 0; i < input_actions.size(); i++)
    {
        lua_pushbbhandle(m_context.GetState(), input_actions[i].handle.handle);
        lua_setglobal(m_context.GetState(), input_actions[i].name.c_str());
    }
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

void LuaECSEngine::LoadInputFunctions(const InputChannelHandle a_channel)
{
    lua_pushbbhandle(m_context.GetState(), a_channel.handle);

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
    lua_pushvalue(m_context.GetState(), -1);
    lua_pushcclosure(m_context.GetState(), a_function, 1);
    lua_setglobal(m_context.GetState(), a_func_name);
}
