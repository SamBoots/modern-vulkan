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

    void* ret_val = nullptr;

    if (a_new_size)
    {
        ret_val = allocator->Alloc(a_new_size, 8);
        BB_ASSERT(ret_val, "failed to allocate lua mem");

        if (a_ptr)
        {
            memcpy(ret_val, a_ptr, Min(a_old_size, a_new_size));
            allocator->Free(a_ptr);
        }
    }
    else if (a_ptr)
    {
        allocator->Free(a_ptr);
    }

    return ret_val;
}

#define _USE_LAUL

void LuaContext::RegisterLua()
{
#ifdef _USE_LAUL
    m_state = luaL_newstate();
#else
    m_state = lua_newstate(LuaAlloc, &m_allocator);
#endif // _USE_LUAL
    luaL_openlibs(m_state);
    lua_registerbbtypes(m_state);
    PathString lua_path = Asset::GetAssetPath();
    lua_path.append("lua\\?.lua");
    AddIncludePath(lua_path.GetView());
}

bool LuaContext::Init(MemoryArena& a_arena, const size_t a_lua_mem_size)
{
    if (m_state != nullptr)
        return false;
    m_allocator.Initialize(a_arena, a_lua_mem_size);
    RegisterLua();
    return true;
}

bool LuaContext::Reset()
{
    if (m_state == nullptr)
        return false;
    lua_close(m_state);
    RegisterLua();
    return true;
}

bool LuaContext::LoadLuaFile(MemoryArena& a_temp_arena, const StringView& a_file_path)
{
    const Buffer buffer = OSReadFile(a_temp_arena, a_file_path.c_str());
    return LoadBuffer(buffer);
}

bool LuaContext::LoadLuaDirectory(MemoryArena& a_temp_arena, const StringView& a_file_path)
{
    LuaStackScope scope(m_state);
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
        bool status = LoadLuaFile(a_temp_arena, path.GetView());

        if (!status)
        {
            BB_WARNING(false, lua_tostring(m_state, -1), WarningType::HIGH);
            return false;
        }
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

struct LuaReadBuffer
{
    Buffer buff;
    bool read;
};

static const char* LuaLoadReader(lua_State*, void* a_data, size_t* a_size)
{
    LuaReadBuffer* buf = reinterpret_cast<LuaReadBuffer*>(a_data);
    if (buf->read) {
        *a_size = 0;
        return nullptr;  // Signal end of input
    }
    buf->read = true;
    *a_size = buf->buff.size;
    return reinterpret_cast<const char*>(buf->buff.data);
}

bool LuaContext::LoadBuffer(Buffer a_string)
{
    LuaReadBuffer read_buff;
    read_buff.buff = a_string;
    read_buff.read = false;
    int success = lua_load(m_state, LuaLoadReader, reinterpret_cast<void*>(&a_string), "LoadBuffer function", "t");
    if (success != LUA_OK)
        return false;

    success = lua_pcall(m_state, 0, LUA_MULTRET, 0);
    if (success != LUA_OK)
        return false;

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

bool LuaContext::CallFunction(lua_CFunction a_function, const int a_nargs, const int a_nresults)
{
    lua_pushcfunction(m_state, a_function);
    return CallFunction(a_nargs, a_nresults);
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
