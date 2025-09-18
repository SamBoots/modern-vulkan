#include "EditorGame.hpp"
#include "InputSystem.hpp"

using namespace BB;

bool EditorGame::Init(const uint2 a_viewport_size, const StringView a_project_name, MemoryArena* a_parena, const ConstSlice<PFN_LuaPluginRegisterFunctions> a_register_funcs)
{
    m_editor_mode = false;
    return m_game.Init(a_viewport_size, a_project_name, a_parena, a_register_funcs);
}

bool EditorGame::Update(const float a_delta_time, const bool a_toggle_editor_mode, const CameraInput& a_camera_input, const bool a_selected)
{
    if (a_toggle_editor_mode)
        ToggleEditorMode();
    bool game_input = a_selected;
    if (m_editor_mode)
    {
        game_input = false;
        EditorUpdate(a_delta_time, a_camera_input);
    }

    return m_game.Update(a_delta_time, game_input);
}

void EditorGame::Destroy()
{
    m_game.Destroy();
}

bool EditorGame::Reload()
{
    return m_game.Reload();
}

void EditorGame::ToggleEditorMode()
{
    m_editor_mode = !m_editor_mode;

    float3 right;
    float3 up;
    float3 forward;
    Float4x4ExtractView(m_game.GetCameraView(), right, up, forward);

    m_camera.SetPosition(m_game.GetCameraPos());
    m_camera.SetYawPitchFromForward(forward);
}

void EditorGame::EditorUpdate(const float a_delta_time, const CameraInput& a_camera_input)
{
    const float2 move = Input::InputActionGetFloat2(a_camera_input.channel, a_camera_input.move) * a_delta_time;
    const float3 player_move = float3(move.x, 0, move.y);
    const float wheel_move = Input::InputActionGetFloat(a_camera_input.channel, a_camera_input.move_speed_slider);

    m_camera.AddSpeed(wheel_move);
    m_camera.Move(player_move);

    if (Input::InputActionIsHeld(a_camera_input.channel, a_camera_input.enable_rotate))
    {
        constexpr float LOOK_SENSITIVITY = 0.002f;
        const float2 look = Input::InputActionGetFloat2(a_camera_input.channel, a_camera_input.look_around) * LOOK_SENSITIVITY;
        m_camera.Rotate(look.x, look.y);
    }

    m_camera.Update(a_delta_time);
}

bool EditorGame::IsEditorMode() const
{
    return m_editor_mode;
}

GameInstance& EditorGame::GetGameInstance()
{
    return m_game;
}

float3 EditorGame::GetCameraPos()
{
    if (m_editor_mode)
    {
        return m_camera.GetPosition();
    }
    return m_game.GetCameraPos();
}

float4x4 EditorGame::GetCameraView()
{
    if (m_editor_mode)
    {
        return m_camera.CalculateView();
    }
    return m_game.GetCameraView();
}
