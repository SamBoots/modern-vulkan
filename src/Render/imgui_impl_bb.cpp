// dear imgui: Renderer Backend for CrossRenderer, referenced of the imgui backend base for Vulkan.

#include "imgui_impl_bb.hpp"
#include "Program.h"

#include "shared_common.hlsl.h"

using namespace BB;

struct ImGui_ImplBB_Data
{
    BB::WindowHandle            window;
    int                         MouseTrackedArea;   // 0: not tracked, 1: client are, 2: non-client area
    int                         MouseButtonsDown;
    int64_t                     Time;
    int64_t                     TicksPerSecond;
    ImGuiMouseCursor            LastMouseCursor;

    ImGui_ImplBB_Data() { memset(this, 0, sizeof(*this)); }
};

//-----------------------------------------------------------------------------
// FUNCTIONS
//-----------------------------------------------------------------------------

static ImGui_ImplBB_Data* ImGui_ImplBB_GetPlatformData()
{
    return ImGui::GetCurrentContext() ? reinterpret_cast<ImGui_ImplBB_Data*>(ImGui::GetIO().BackendPlatformUserData) : nullptr;
}

//BB FRAMEWORK TEMPLATE, MAY CHANGE THIS.
static ImGuiKey ImGui_ImplBB_KEYBOARD_KEYToImGuiKey(const KEYBOARD_KEY a_Key)
{
    switch (a_Key)
    {
    case KEYBOARD_KEY::TAB: return ImGuiKey_Tab;
    case KEYBOARD_KEY::BACKSPACE: return ImGuiKey_Backspace;
    case KEYBOARD_KEY::SPACEBAR: return ImGuiKey_Space;
    case KEYBOARD_KEY::RETURN: return ImGuiKey_Enter;
    case KEYBOARD_KEY::ESCAPE: return ImGuiKey_Escape;
    case KEYBOARD_KEY::APOSTROPHE: return ImGuiKey_Apostrophe;
    case KEYBOARD_KEY::COMMA: return ImGuiKey_Comma;
    case KEYBOARD_KEY::MINUS: return ImGuiKey_Minus;
    case KEYBOARD_KEY::PERIOD: return ImGuiKey_Period;
    case KEYBOARD_KEY::SLASH: return ImGuiKey_Slash;
    case KEYBOARD_KEY::SEMICOLON: return ImGuiKey_Semicolon;
    case KEYBOARD_KEY::EQUALS: return ImGuiKey_Equal;
    case KEYBOARD_KEY::BRACKETLEFT: return ImGuiKey_LeftBracket;
    case KEYBOARD_KEY::BACKSLASH: return ImGuiKey_Backslash;
    case KEYBOARD_KEY::BRACKETRIGHT: return ImGuiKey_RightBracket;
    case KEYBOARD_KEY::GRAVE: return ImGuiKey_GraveAccent;
    case KEYBOARD_KEY::CAPSLOCK: return ImGuiKey_CapsLock;
    case KEYBOARD_KEY::NUMPADMULTIPLY: return ImGuiKey_KeypadMultiply;
    case KEYBOARD_KEY::SHIFTLEFT: return ImGuiKey_LeftShift;
    case KEYBOARD_KEY::CONTROLLEFT: return ImGuiKey_LeftCtrl;
    case KEYBOARD_KEY::ALTLEFT: return ImGuiKey_LeftAlt;
    case KEYBOARD_KEY::SHIFTRIGHT: return ImGuiKey_RightShift;
    case KEYBOARD_KEY::KEY_0: return ImGuiKey_0;
    case KEYBOARD_KEY::KEY_1: return ImGuiKey_1;
    case KEYBOARD_KEY::KEY_2: return ImGuiKey_2;
    case KEYBOARD_KEY::KEY_3: return ImGuiKey_3;
    case KEYBOARD_KEY::KEY_4: return ImGuiKey_4;
    case KEYBOARD_KEY::KEY_5: return ImGuiKey_5;
    case KEYBOARD_KEY::KEY_6: return ImGuiKey_6;
    case KEYBOARD_KEY::KEY_7: return ImGuiKey_7;
    case KEYBOARD_KEY::KEY_8: return ImGuiKey_8;
    case KEYBOARD_KEY::KEY_9: return ImGuiKey_9;
    case KEYBOARD_KEY::A: return ImGuiKey_A;
    case KEYBOARD_KEY::B: return ImGuiKey_B;
    case KEYBOARD_KEY::C: return ImGuiKey_C;
    case KEYBOARD_KEY::D: return ImGuiKey_D;
    case KEYBOARD_KEY::E: return ImGuiKey_E;
    case KEYBOARD_KEY::F: return ImGuiKey_F;
    case KEYBOARD_KEY::G: return ImGuiKey_G;
    case KEYBOARD_KEY::H: return ImGuiKey_H;
    case KEYBOARD_KEY::I: return ImGuiKey_I;
    case KEYBOARD_KEY::J: return ImGuiKey_J;
    case KEYBOARD_KEY::K: return ImGuiKey_K;
    case KEYBOARD_KEY::L: return ImGuiKey_L;
    case KEYBOARD_KEY::M: return ImGuiKey_M;
    case KEYBOARD_KEY::N: return ImGuiKey_N;
    case KEYBOARD_KEY::O: return ImGuiKey_O;
    case KEYBOARD_KEY::P: return ImGuiKey_P;
    case KEYBOARD_KEY::Q: return ImGuiKey_Q;
    case KEYBOARD_KEY::R: return ImGuiKey_R;
    case KEYBOARD_KEY::S: return ImGuiKey_S;
    case KEYBOARD_KEY::T: return ImGuiKey_T;
    case KEYBOARD_KEY::U: return ImGuiKey_U;
    case KEYBOARD_KEY::V: return ImGuiKey_V;
    case KEYBOARD_KEY::W: return ImGuiKey_W;
    case KEYBOARD_KEY::X: return ImGuiKey_X;
    case KEYBOARD_KEY::Y: return ImGuiKey_Y;
    case KEYBOARD_KEY::Z: return ImGuiKey_Z;
    default: return ImGuiKey_None;
    }
}

//On true means that imgui takes the input and doesn't give it to the engine.
bool BB::ImGui_ImplBB_ProcessInput(const BB::InputEvent& a_input_event)
{
    ImGuiIO& io = ImGui::GetIO();
    if (a_input_event.input_type == INPUT_TYPE::MOUSE)
    {
        const BB::MouseInfo& mouse_info = a_input_event.mouse_info;
        io.AddMousePosEvent(mouse_info.mouse_pos.x, mouse_info.mouse_pos.y);
        if (a_input_event.mouse_info.wheel_move != 0)
        {
            io.AddMouseWheelEvent(0.0f, static_cast<float>(a_input_event.mouse_info.wheel_move));
        }

        constexpr int left_button = 0;
        constexpr int right_button = 1;
        constexpr int middle_button = 2;

        if (mouse_info.left_pressed)
            io.AddMouseButtonEvent(left_button, true);
        if (mouse_info.right_pressed)
            io.AddMouseButtonEvent(right_button, true);
        if (mouse_info.middle_pressed)
            io.AddMouseButtonEvent(middle_button, true);

        if (mouse_info.left_released)
            io.AddMouseButtonEvent(left_button, false);
        if (mouse_info.right_released)
            io.AddMouseButtonEvent(right_button, false);
        if (mouse_info.middle_released)
            io.AddMouseButtonEvent(middle_button, false);

        return io.WantCaptureMouse;
    }
    else if (a_input_event.input_type == INPUT_TYPE::KEYBOARD)
    {
        const BB::KeyInfo& key_info = a_input_event.key_info;
        const ImGuiKey imgui_key = ImGui_ImplBB_KEYBOARD_KEYToImGuiKey(key_info.scan_code);

        io.AddKeyEvent(imgui_key, key_info.key_pressed);
        //THIS IS WRONG! It gives no UTF16 character.
        //But i'll keep it in here to test if imgui input actually works.
        io.AddInputCharacterUTF16(static_cast<ImWchar16>(key_info.scan_code));

        return io.WantCaptureKeyboard;
    }

    return false;
}
