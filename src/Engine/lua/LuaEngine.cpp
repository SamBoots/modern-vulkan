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

bool LuaContext::Init(MemoryArena& a_arena, const size_t a_lua_mem_size)
{
    if (m_state != nullptr)
        return false;
    m_allocator.Initialize(a_arena, a_lua_mem_size);
    m_state = luaL_newstate();

    luaL_openlibs(m_state);

    LuaStackScope scope(m_state);
    lua_registerbbtypes(m_state);
    PathString lua_path = Asset::GetAssetPath();
    lua_path.append("lua\\?.lua");
    AddIncludePath(lua_path.GetView());

    return true;
}

bool LuaContext::LoadLuaFile(const StringView& a_file_path)
{
    return luaL_dofile(m_state, a_file_path.c_str()) == LUA_OK;
}

bool LuaContext::LoadLuaDirectory(MemoryArena& a_temp_arena, const StringView& a_file_path)
{
    ConstSlice<StackString<MAX_PATH_SIZE>> lua_paths;
    PathString lua_path = a_file_path;
    lua_path.append("\\*lua");
    if (!OSGetDirectoryEntries(a_temp_arena, lua_path.c_str(), lua_paths))
        return false;

    for (size_t i = 0; i < lua_paths.size(); i++)
    {
        PathString path = a_file_path;
        path.push_directory_slash();
        path.append(lua_paths[i].GetView());
        bool status = LoadLuaFile(path.GetView());
        BB_ASSERT(status == true, lua_tostring(m_state, -1));
    }

    return true;
}

bool LuaContext::RegisterActionHandlesLua(const InputChannelHandle a_channel)
{
    const Slice input_actions = Input::GetAllInputActions(a_channel);
    LuaStackScope scope(m_state);
    for (size_t i = 0; i < input_actions.size(); i++)
    {
        lua_pushbbhandle(m_state, input_actions[i].handle.handle);
        lua_setglobal(m_state, input_actions[i].name.c_str());
    }
    return true;
}

bool LuaContext::AddIncludePath(const StringView a_path)
{
    StackString<2024> lua_include_paths;
    LuaStackScope scope(m_state);

    lua_getglobal(m_state, "package");
    lua_getfield(m_state, -1, "path");
    const char* current_path = lua_tostring(m_state, -1);
    lua_include_paths.append(current_path);
    lua_include_paths.append(";");
    lua_include_paths.append(a_path);
    lua_pop(m_state, 1);

    lua_pushstring(m_state, lua_include_paths.c_str());
    lua_setfield(m_state, -2, "path");
    return true;
}

bool LuaContext::LoadAndCallFunction(const char* a_function, const int a_nresults)
{
    if (!LoadFunction(a_function))
        return false;
    return CallFunction(0, a_nresults);
}

bool LuaContext::LoadFunction(const char* a_function)
{
    return VerifyGlobal(a_function);
}

bool LuaContext::CallFunction(const int a_nargs, const int a_nresults)
{
    int status = lua_pcall(m_state, a_nargs, a_nresults, 0);
    if (status == LUA_OK)
        return true;

    BB_WARNING(false, lua_tostring(m_state, -1), WarningType::HIGH);

    return false;
}

bool LuaContext::VerifyGlobal(const char* a_function)
{
    if (lua_getglobal(m_state, a_function) == LUA_TNIL)
    {
        BB_WARNING(false, a_function, WarningType::HIGH);
        return false;
    }
    return true;
}
