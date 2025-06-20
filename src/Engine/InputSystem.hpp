#pragma once
#include "Enginefwd.hpp"
#include "HID.h"
#include "Storage/BBString.h"
#include "Storage/FixedArray.h"

namespace BB
{
    using InputActionName = StackString<32>;
    using InputChannelName = StackString<32>;
    
#define INPUT_VALUE_TYPE_D(FUNC, ...) \
    FUNC(BOOL, 0, __VA_ARGS__) \
    FUNC(FLOAT, 1, __VA_ARGS__) \
    FUNC(FLOAT_2, 2, __VA_ARGS__)

#define INPUT_BINDING_TYPE_D(FUNC, ...) \
    FUNC(BINDING, 0, __VA_ARGS__) \
    FUNC(COMPOSITE_UP_DOWN_RIGHT_LEFT, 1, __VA_ARGS__)  // input_keys[] , 0 = UP, 1, = DOWN, 2 = RIGHT, 3 = LEFT

#define INPUT_SOURCE_D(FUNC, ...) \
    FUNC(KEYBOARD, 0, __VA_ARGS__) \
    FUNC(MOUSE, 1, __VA_ARGS__)

#define MOUSE_INPUT_D(FUNC, ...) \
    FUNC(LEFT_BUTTON, 0, __VA_ARGS__) \
    FUNC(RIGHT_BUTTON, 1, __VA_ARGS__) \
    FUNC(MIDDLE_BUTTON, 2, __VA_ARGS__) \
    FUNC(SCROLL_WHEEL, 3, __VA_ARGS__) \
    FUNC(MOUSE_MOVE, 4, __VA_ARGS__)

    BB_CREATE_ENUM(INPUT_VALUE_TYPE, uint32_t, INPUT_VALUE_TYPE_D)
    BB_CREATE_ENUM(INPUT_BINDING_TYPE, uint32_t, INPUT_BINDING_TYPE_D)
    BB_CREATE_ENUM(INPUT_SOURCE, uint32_t, INPUT_SOURCE_D)
    BB_CREATE_ENUM(MOUSE_INPUT, uint32_t, MOUSE_INPUT_D)

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
        INPUT_BINDING_TYPE binding_type;
        INPUT_SOURCE source;
        FixedArray<InputKey, 4> input_keys;
    };

    struct InputAction
    {
        InputActionHandle handle;
        InputActionName name;
        INPUT_VALUE_TYPE value_type;
        INPUT_BINDING_TYPE binding_type;
        INPUT_SOURCE input_source;
        FixedArray<InputKey, 4> input_keys;
    };

    namespace Input
    {
        bool InitInputSystem(struct MemoryArena& a_arena);
        void UpdateInput(const ConstSlice<InputEvent> a_input_events);

        InputChannelHandle CreateInputChannel(MemoryArena& a_arena, const InputChannelName& a_channel_name, const uint32_t a_max_actions);

        float2 GetMousePos(const WindowHandle a_window_handle);

        InputActionHandle CreateInputAction(const InputChannelHandle a_channel, const InputActionName& a_name, const InputActionCreateInfo& a_create_info);
        InputActionHandle FindInputAction(const InputChannelHandle a_channel, const InputActionName& a_name);

        const InputChannelName& GetInputChannelName(const InputChannelHandle a_channel);
        const InputActionName& GetInputActionName(const InputChannelHandle a_channel, const InputActionHandle a_input_action);

        bool InputActionIsPressed(const InputChannelHandle a_channel, const InputActionHandle a_input_action);
        bool InputActionIsHeld(const InputChannelHandle a_channel, const InputActionHandle a_input_action);
        bool InputActionIsReleased(const InputChannelHandle a_channel, const InputActionHandle a_input_action);
        float InputActionGetFloat(const InputChannelHandle a_channel, const InputActionHandle a_input_action);
        float2 InputActionGetFloat2(const InputChannelHandle a_channel, const InputActionHandle a_input_action);
        Slice<InputAction> GetAllInputActions(const InputChannelHandle a_channel);
    }
}
