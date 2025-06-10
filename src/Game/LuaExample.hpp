#pragma once
#include "ViewportInterface.hpp"
#include "Camera.hpp"
#include "Storage/Array.h"
#include "Enginefwd.hpp"
#include "SceneHierarchy.hpp"
#include "lua/LuaEngine.hpp"

namespace BB
{
    class LuaExample
    {
    public:
        bool Init(const uint2 a_game_viewport_size, const uint32_t a_back_buffer_count);
        bool Update(const float a_delta_time, const bool a_selected) {return false;}
        // maybe ifdef this for editor
        void DisplayImGuiInfo() {}
        void Destroy() {}

        float3 GetCameraPos() const { return float3(0.f); }

        Viewport& GetViewport() { return m_viewport; }
        SceneHierarchy& GetSceneHierarchy() { return m_scene_hierarchy; }

    private:
        MemoryArena m_memory;
        Viewport m_viewport;
        SceneHierarchy m_scene_hierarchy;
        LuaContext m_context;
    };
    static_assert(is_interactable_viewport_interface<LuaExample>);
}
