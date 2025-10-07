#pragma once
#include "GameInstance.hpp"
#include "FreeCamera.hpp"

namespace BB
{
    struct CameraInput
    {
        InputChannelHandle channel;
        InputActionHandle move;
        InputActionHandle look_around;
        InputActionHandle enable_rotate;
        InputActionHandle move_speed_slider;
    };

    class EditorGame
    {
    public:
        bool Init(const uint2 a_viewport_size, const StringView a_project_name, MemoryArena* a_parena, const ConstSlice<PFN_LuaPluginRegisterFunctions> a_register_funcs = ConstSlice<PFN_LuaPluginRegisterFunctions>());
        bool Update(const float a_delta_time, const bool a_toggle_editor_mode, const CameraInput& a_camera_input, const bool a_selected = true);
        void Destroy();

        bool Reload();

        bool IsEditorMode() const;
        GameInstance& GetGameInstance();
        float3 GetCameraPos();
        float4x4 GetCameraView();

        Viewport& GetViewport() { return m_game.GetViewport(); }
        InputChannelHandle GetInputChannel() const { return m_game.GetInputChannel(); }
        SceneHierarchy& GetSceneHierarchy() { return m_game.GetSceneHierarchy(); }
        const StringView GetGameEditorName() const { return m_project_editor_name.GetView(); }
        const StringView GetGameName() const {return m_game.GetGameName(); }

    private:
        void ToggleEditorMode();
        void EditorUpdate(const float a_delta_time, const CameraInput& a_camera_input);

        StackString<48> m_project_editor_name;
        GameInstance m_game;
        FreeCamera m_camera;
        bool m_editor_mode;
    };
}
