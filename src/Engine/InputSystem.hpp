#pragma once
#include "Enginefwd.hpp"
#include "HID.h"
#include "Storage/BBString.h"
#include "Utils/Slice.h"

namespace BB
{
    using InputActionName = StackString<32>;

    enum class INPUT_VALUE_TYPE : uint32_t
    {
        BOOL,
        FLOAT,
        FLOAT_2
    };

    enum class INPUT_ACTION_TYPE : uint32_t
    {
        VALUE,
        BUTTON,
        DIRECT
    };

    enum class INPUT_BINDING_TYPE : uint32_t
    {
        BINDING,
        COMPOSITE_UP_DOWN_RIGHT_LEFT, // input_keys[] , 0 = UP, 1, = DOWN, 2 = RIGHT, 3 = LEFT
    };

    enum class INPUT_SOURCE
    {
        KEYBOARD,
        MOUSE
    };

    enum class MOUSE_INPUT : uint32_t
    {
        LEFT_BUTTON,
        RIGHT_BUTTON,
        MIDDLE_BUTTON,
        SCROLL_WHEEL,
        MOUSE_MOVE
    };

    struct InputKey
    {
        union
        {
            KEYBOARD_KEY keyboard_key;
            MOUSE_INPUT mouse_input;
        };

    };

    struct InputActionCreateInfo
    {
        INPUT_VALUE_TYPE value_type;
        INPUT_ACTION_TYPE action_type;
        INPUT_BINDING_TYPE binding_type;
        INPUT_SOURCE source;
        FixedArray<InputKey, 4> input_keys;
    };

    namespace Input
    {
        bool InitInputSystem(struct MemoryArena& a_arena, const uint32_t a_max_actions);
        void UpdateInput(const ConstSlice<InputEvent> a_input_events);

        InputActionHandle CreateInputAction(const InputActionName& a_name, const InputActionCreateInfo& a_create_info);
        InputActionHandle FindInputAction(const InputActionName& a_name);

        bool InputActionIsPressed(const InputActionHandle a_input_action);
        float InputActionGetFloat(const InputActionHandle a_input_action);
        float2 InputActionGetFloat2(const InputActionHandle a_input_action);
    }
}
