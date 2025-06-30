#pragma once
#include "ViewportInterface.hpp"
#include "Enginefwd.hpp"
#include "SceneHierarchy.hpp"
#include "lua/LuaEngine.hpp"

namespace BB
{
    typedef void (*PFN_LuaPluginRegisterFunctions)(class GameInstance& a_inst);

    class GameInstance
    {
    public:
        bool Init(const uint2 a_viewport_size, const StringView a_project_name, MemoryArena* a_parena, const ConstSlice<PFN_LuaPluginRegisterFunctions> a_register_funcs = ConstSlice<PFN_LuaPluginRegisterFunctions>());
        bool Update(const float a_delta_time, const bool a_selected = true);
        void Destroy();

        bool Verify();

        bool Reload();
        bool IsDirty() const;
        void SetDirty();

        float3 GetCameraPos();
        float4x4 GetCameraView();
        Viewport& GetViewport() { return m_viewport; }
        InputChannelHandle GetInputChannel() const { return m_input_channel; }
        SceneHierarchy& GetSceneHierarchy() { return m_scene_hierarchy; }
        MemoryArena& GetMemory() { return m_arena; }
        const PathString& GetProjectPath() const { return m_project_path; }
        struct lua_State* GetLuaState();

    private:
        bool InitLua();
        void RegisterLuaCFunctions();

        bool m_dirty = false;

        MemoryArena m_arena;
        Viewport m_viewport;
        SceneHierarchy m_scene_hierarchy;
        LuaContext m_lua;
        InputChannelHandle m_input_channel;
        PathString m_project_path;
        StaticArray<PFN_LuaPluginRegisterFunctions> m_plugin_functions;
    };
}
