#pragma once
#include "Enginefwd.hpp"
#include "HID.h"
#include "Storage/BBString.h"
#include "Storage/FixedArray.h"

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

    static inline const char* INPUT_VALUE_TYPE_STR(const INPUT_VALUE_TYPE a_enum)
    {
        switch (a_enum)
        {
            ENUM_CASE_STR(INPUT_VALUE_TYPE, BOOL);
            ENUM_CASE_STR(INPUT_VALUE_TYPE, FLOAT);
            ENUM_CASE_STR(INPUT_VALUE_TYPE, FLOAT_2);
            ENUM_CASE_STR_NOT_FOUND();
        }
    }

    static inline const char* INPUT_ACTION_TYPE_STR(const INPUT_ACTION_TYPE a_enum)
    {
        switch (a_enum)
        {
            ENUM_CASE_STR(INPUT_ACTION_TYPE, VALUE);
            ENUM_CASE_STR(INPUT_ACTION_TYPE, BUTTON);
            ENUM_CASE_STR(INPUT_ACTION_TYPE, DIRECT);
            ENUM_CASE_STR_NOT_FOUND();
        }
    }

    static inline const char* INPUT_BINDING_TYPE_STR(const INPUT_BINDING_TYPE a_enum)
    {
        switch (a_enum)
        {
            ENUM_CASE_STR(INPUT_BINDING_TYPE, BINDING);
            ENUM_CASE_STR(INPUT_BINDING_TYPE, COMPOSITE_UP_DOWN_RIGHT_LEFT);
            ENUM_CASE_STR_NOT_FOUND();
        }
    }

    static inline const char* INPUT_SOURCE_STR(const INPUT_SOURCE a_enum)
    {
        switch (a_enum)
        {
            ENUM_CASE_STR(INPUT_SOURCE, KEYBOARD);
            ENUM_CASE_STR(INPUT_SOURCE, MOUSE);
            ENUM_CASE_STR_NOT_FOUND();
        }
    }

    static inline const char* MOUSE_INPUT_STR(const MOUSE_INPUT a_enum)
    {
        switch (a_enum)
        {
            ENUM_CASE_STR(MOUSE_INPUT, LEFT_BUTTON);
            ENUM_CASE_STR(MOUSE_INPUT, RIGHT_BUTTON);
            ENUM_CASE_STR(MOUSE_INPUT, MIDDLE_BUTTON);
            ENUM_CASE_STR(MOUSE_INPUT, SCROLL_WHEEL);
            ENUM_CASE_STR(MOUSE_INPUT, MOUSE_MOVE);
            ENUM_CASE_STR_NOT_FOUND();
        }
    }

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

    struct InputAction
    {
        InputActionName name;
        INPUT_VALUE_TYPE value_type;
        INPUT_ACTION_TYPE action_type;
        INPUT_BINDING_TYPE binding_type;
        INPUT_SOURCE input_source;
        FixedArray<InputKey, 4> input_keys;
    };

    namespace Input
    {
        bool InitInputSystem(struct MemoryArena& a_arena, const uint32_t a_max_actions);
        void UpdateInput(const ConstSlice<InputEvent> a_input_events);

        float2 GetMousePos(const WindowHandle a_window_handle);

        InputActionHandle CreateInputAction(const InputActionName& a_name, const InputActionCreateInfo& a_create_info);
        InputActionHandle FindInputAction(const InputActionName& a_name);

        bool InputActionIsPressed(const InputActionHandle a_input_action);
        bool InputActionIsHeld(const InputActionHandle a_input_action);
        bool InputActionIsReleased(const InputActionHandle a_input_action);
        float InputActionGetFloat(const InputActionHandle a_input_action);
        float2 InputActionGetFloat2(const InputActionHandle a_input_action);
        Slice<InputAction> GetAllInputActions();
    }
}
