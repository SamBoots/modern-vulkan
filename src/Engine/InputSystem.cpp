#include "InputSystem.hpp"
#include "Storage/Hashmap.h"
#include "Storage/FixedArray.h"
#include "Storage/Slotmap.h"
#include "Storage/Array.h"
#include "OS/Program.h"

#include "Math/Math.inl"

using namespace BB;

constexpr uint32_t COMPOSITE_UP = 0;
constexpr uint32_t COMPOSITE_DOWN = 1;
constexpr uint32_t COMPOSITE_RIGHT = 2;
constexpr uint32_t COMPOSITE_LEFT = 3;

struct InputSystem
{
    StaticOL_HashMap<InputActionName, InputActionHandle> input_action_index_map;
    StaticSlotmap<InputAction, InputActionHandle> input_actions;

    struct State
    {
        FixedArray<bool, static_cast<uint32_t>(KEYBOARD_KEY::ENUM_SIZE)> keyboard_state;
        struct Mouse
        {
            float2 mouse_move;
            float wheel_move;
            bool left_pressed;
            bool right_pressed;
            bool middle_pressed;
        } mouse_state;
    };
    State current;
    State previous;
};

static InputSystem* s_input_system;

enum class KEY_STATE
{
    IDLE,
    PRESSED,
    RELEASED,
    HELD
};

static inline bool KeyboardKeyDownOrPressed(const KEYBOARD_KEY a_key)
{
    const uint32_t key_index = static_cast<uint32_t>(a_key);
    return s_input_system->current.keyboard_state[key_index];
}

static inline KEY_STATE GetKeyboardKeyState(const KEYBOARD_KEY a_key)
{
    const uint32_t key_index = static_cast<uint32_t>(a_key);
    if (s_input_system->current.keyboard_state[key_index] && s_input_system->previous.keyboard_state[key_index])
        return KEY_STATE::HELD;
    if (s_input_system->current.keyboard_state[key_index])
        return KEY_STATE::PRESSED;
    if (s_input_system->previous.keyboard_state[key_index])
        return KEY_STATE::RELEASED;

    return KEY_STATE::IDLE;
}

static inline KEY_STATE MouseButtonState(const MOUSE_INPUT a_input)
{
    switch (a_input)
    {
        case MOUSE_INPUT::LEFT_BUTTON:
            if (s_input_system->current.mouse_state.left_pressed && s_input_system->previous.mouse_state.left_pressed)
                return KEY_STATE::HELD;
            if (s_input_system->current.mouse_state.left_pressed)
                return KEY_STATE::PRESSED;
            if (s_input_system->previous.mouse_state.left_pressed)
                return KEY_STATE::RELEASED;
        break;
        case MOUSE_INPUT::MIDDLE_BUTTON:
            if (s_input_system->current.mouse_state.middle_pressed && s_input_system->previous.mouse_state.middle_pressed)
                return KEY_STATE::HELD;
            if (s_input_system->current.mouse_state.middle_pressed)
                return KEY_STATE::PRESSED;
            if (s_input_system->previous.mouse_state.middle_pressed)
                return KEY_STATE::RELEASED;
            break;
        case MOUSE_INPUT::RIGHT_BUTTON:
            if (s_input_system->current.mouse_state.right_pressed && s_input_system->previous.mouse_state.right_pressed)
                return KEY_STATE::HELD;
            if (s_input_system->current.mouse_state.right_pressed)
                return KEY_STATE::PRESSED;
            if (s_input_system->previous.mouse_state.right_pressed)
                return KEY_STATE::RELEASED;
            break;
    default:
        BB_WARNING(false, "checking a mouse button state but MOUSE_INPUT is not a button", WarningType::MEDIUM);
        break;
    }

    return KEY_STATE::IDLE;
}

bool Input::InitInputSystem(struct MemoryArena& a_arena, const uint32_t a_max_actions)
{
    if (s_input_system)
        return false;
    s_input_system = ArenaAllocType(a_arena, InputSystem);
    s_input_system->input_action_index_map.Init(a_arena, a_max_actions);
    s_input_system->input_actions.Init(a_arena, a_max_actions);
    return true;
}

void Input::UpdateInput(const ConstSlice<InputEvent> a_input_events)
{
    // remember the previous frame
    s_input_system->previous = s_input_system->current;

    float2 mouse_move = float2(0);
    float wheel_move = 0.f;
    for (size_t i = 0; i < a_input_events.size(); i++)
    {
        const InputEvent& ev = a_input_events[i];
        if (ev.input_type == INPUT_TYPE::KEYBOARD)
        {
            s_input_system->current.keyboard_state[static_cast<uint32_t>(ev.key_info.scan_code)] =  ev.key_info.key_pressed;
        }
        else if (ev.input_type == INPUT_TYPE::MOUSE)
        {
            mouse_move = mouse_move + ev.mouse_info.move_offset;
            wheel_move = wheel_move + static_cast<float>(ev.mouse_info.wheel_move);
            if (ev.mouse_info.left_pressed)
                s_input_system->current.mouse_state.left_pressed = true;
            if (ev.mouse_info.right_pressed)
                s_input_system->current.mouse_state.right_pressed = true;
            if (ev.mouse_info.middle_pressed)
                s_input_system->current.mouse_state.middle_pressed = true;

            if (ev.mouse_info.left_released)
                s_input_system->current.mouse_state.left_pressed = false;
            if (ev.mouse_info.right_released)
                s_input_system->current.mouse_state.right_pressed = false;
            if (ev.mouse_info.middle_released)
                s_input_system->current.mouse_state.middle_pressed = false;
        }
    }
    s_input_system->current.mouse_state.mouse_move = mouse_move;
    s_input_system->current.mouse_state.wheel_move = wheel_move;
}

float2 Input::GetMousePos(const WindowHandle a_window_handle)
{
    return OSGetCursorPos(a_window_handle);
}

InputActionHandle Input::CreateInputAction(const InputActionName& a_name, const InputActionCreateInfo& a_create_info)
{
    if (s_input_system->input_action_index_map.find(a_name))
        return InputActionHandle(); // already found

    InputAction action{};
    action.name = a_name;
    action.value_type = a_create_info.value_type;
    action.action_type = a_create_info.action_type;
    action.binding_type = a_create_info.binding_type;
    action.input_source = a_create_info.source;
    action.input_keys = a_create_info.input_keys;

    const InputActionHandle handle = s_input_system->input_actions.emplace(action);
    s_input_system->input_action_index_map.emplace(a_name, handle);

    return handle;
}

InputActionHandle Input::FindInputAction(const InputActionName& a_name)
{
    if (const InputActionHandle* found = s_input_system->input_action_index_map.find(a_name))
        return *found;

    return InputActionHandle(); // invalid
}

static bool _InputActionCheck(const InputActionHandle a_input_action, const KEY_STATE a_state)
{
    const InputAction& ia = s_input_system->input_actions.find(a_input_action);
    BB_WARNING(ia.value_type == INPUT_VALUE_TYPE::BOOL, "trying to get bool from an input action that is not made with INPUT_VALUE_TYPE::BOOL", WarningType::HIGH);

    if (ia.input_source == INPUT_SOURCE::KEYBOARD)
        return GetKeyboardKeyState(ia.input_keys[0].keyboard_key) == a_state;

    // do mouse keys as well
    if (ia.input_source == INPUT_SOURCE::MOUSE)
        return MouseButtonState(ia.input_keys[0].mouse_input) == a_state;

    BB_WARNING(false, "input action returned false as it found no path for a key press", WarningType::MEDIUM);
    return false;
}

bool Input::InputActionIsPressed(const InputActionHandle a_input_action)
{
    return _InputActionCheck(a_input_action, KEY_STATE::PRESSED);
}

bool Input::InputActionIsHeld(const InputActionHandle a_input_action)
{
    return _InputActionCheck(a_input_action, KEY_STATE::HELD);
}

bool Input::InputActionIsReleased(const InputActionHandle a_input_action)
{
    return _InputActionCheck(a_input_action, KEY_STATE::RELEASED);
}

float Input::InputActionGetFloat(const InputActionHandle a_input_action)
{
    const InputAction& ia = s_input_system->input_actions.find(a_input_action);
    if (ia.value_type != INPUT_VALUE_TYPE::FLOAT)
    {
        BB_WARNING(false, "trying to get float from an input action that is not made with INPUT_VALUE_TYPE::FLOAT", WarningType::HIGH);
        return 0.f;
    }

    if (ia.input_source == INPUT_SOURCE::MOUSE && ia.input_keys[0].mouse_input == MOUSE_INPUT::SCROLL_WHEEL)
    {
        return s_input_system->current.mouse_state.wheel_move;
    }

    BB_WARNING(false, "input action returned 0.f as it found no path to get the value", WarningType::MEDIUM);
    return 0.f;
}

float2 Input::InputActionGetFloat2(const InputActionHandle a_input_action)
{
    const InputAction& ia = s_input_system->input_actions.find(a_input_action);
    if (ia.value_type != INPUT_VALUE_TYPE::FLOAT_2)
    {
        BB_WARNING(false, "trying to get float2 from an input action that is not made with INPUT_VALUE_TYPE::FLOAT_2", WarningType::HIGH);
        return float2(0.f);
    }

    if (ia.input_source == INPUT_SOURCE::MOUSE && ia.input_keys[0].mouse_input == MOUSE_INPUT::MOUSE_MOVE)
    {
        // TODO, make sure to reverse the Y axis
        return s_input_system->current.mouse_state.mouse_move;
    }

    if (ia.input_source == INPUT_SOURCE::KEYBOARD && ia.binding_type == INPUT_BINDING_TYPE::COMPOSITE_UP_DOWN_RIGHT_LEFT)
    {
        float2 value = float2(0);

        if (KeyboardKeyDownOrPressed(ia.input_keys[COMPOSITE_UP].keyboard_key))
            value.y += 1;
        if (KeyboardKeyDownOrPressed(ia.input_keys[COMPOSITE_DOWN].keyboard_key))
            value.y -= 1;
        if (KeyboardKeyDownOrPressed(ia.input_keys[COMPOSITE_RIGHT].keyboard_key))
            value.x += 1;
        if (KeyboardKeyDownOrPressed(ia.input_keys[COMPOSITE_LEFT].keyboard_key))
            value.x -= 1;

        return value;
    }

    BB_WARNING(false, "input action returned float2(0, 0) as it found no path to get the value", WarningType::MEDIUM);
    return float2(0);
}

Slice<InputAction> Input::GetAllInputActions()
{
    return s_input_system->input_actions.slice();
}
