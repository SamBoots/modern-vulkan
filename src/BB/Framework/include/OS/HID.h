#pragma once
#include "BBEnum.hpp"

namespace BB
{
	constexpr size_t INPUT_EVENT_BUFFER_MAX = 128;

	//These will be translated, the are already close to their real counterpart.
	//Translation table: https://download.microsoft.com/download/1/6/1/161ba512-40e2-4cc9-843a-923143f3456c/translate.pdf
#define KEYBOARD_KEY_D(FUNC, ...) \
		FUNC(NOKEY, 0x00, __VA_ARGS__) \
        FUNC(ESCAPE, 0x01, __VA_ARGS__) \
        FUNC(KEY_1, 0x02, __VA_ARGS__) \
        FUNC(KEY_2, 0x03, __VA_ARGS__) \
        FUNC(KEY_3, 0x04, __VA_ARGS__) \
        FUNC(KEY_4, 0x05, __VA_ARGS__) \
        FUNC(KEY_5, 0x06, __VA_ARGS__) \
        FUNC(KEY_6, 0x07, __VA_ARGS__) \
        FUNC(KEY_7, 0x08, __VA_ARGS__) \
        FUNC(KEY_8, 0x09, __VA_ARGS__) \
        FUNC(KEY_9, 0x0A, __VA_ARGS__) \
        FUNC(KEY_0, 0x0B, __VA_ARGS__) \
        FUNC(MINUS, 0x0C, __VA_ARGS__) \
        FUNC(EQUALS, 0x0D, __VA_ARGS__) \
        FUNC(BACKSPACE, 0x0E, __VA_ARGS__) \
        FUNC(TAB, 0x0F, __VA_ARGS__) \
        FUNC(Q, 0x10, __VA_ARGS__) \
        FUNC(W, 0x11, __VA_ARGS__) \
        FUNC(E, 0x12, __VA_ARGS__) \
        FUNC(R, 0x13, __VA_ARGS__) \
        FUNC(T, 0x14, __VA_ARGS__) \
        FUNC(Y, 0x15, __VA_ARGS__) \
        FUNC(U, 0x16, __VA_ARGS__) \
        FUNC(I, 0x17, __VA_ARGS__) \
        FUNC(O, 0x18, __VA_ARGS__) \
        FUNC(P, 0x19, __VA_ARGS__) \
        FUNC(BRACKETLEFT, 0x1A, __VA_ARGS__) \
        FUNC(BRACKETRIGHT, 0x1B, __VA_ARGS__) \
        /* I think this is non-numpad enter? */ \
        FUNC(RETURN, 0x1C, __VA_ARGS__) \
        FUNC(CONTROLLEFT, 0x1D, __VA_ARGS__) \
        FUNC(A, 0x1E, __VA_ARGS__) \
        FUNC(S, 0x1F, __VA_ARGS__) \
        FUNC(D, 0x20, __VA_ARGS__) \
        FUNC(F, 0x21, __VA_ARGS__) \
        FUNC(G, 0x22, __VA_ARGS__) \
        FUNC(H, 0x23, __VA_ARGS__) \
        FUNC(J, 0x24, __VA_ARGS__) \
        FUNC(K, 0x25, __VA_ARGS__) \
        FUNC(L, 0x26, __VA_ARGS__) \
        FUNC(SEMICOLON, 0x27, __VA_ARGS__) \
        FUNC(APOSTROPHE, 0x28, __VA_ARGS__) \
        FUNC(GRAVE, 0x29, __VA_ARGS__) \
        FUNC(SHIFTLEFT, 0x2A, __VA_ARGS__) \
        FUNC(BACKSLASH, 0x2B, __VA_ARGS__) \
        FUNC(Z, 0x2C, __VA_ARGS__) \
        FUNC(X, 0x2D, __VA_ARGS__) \
        FUNC(C, 0x2E, __VA_ARGS__) \
        FUNC(V, 0x2F, __VA_ARGS__) \
        FUNC(B, 0x30, __VA_ARGS__) \
        FUNC(N, 0x31, __VA_ARGS__) \
        FUNC(M, 0x32, __VA_ARGS__) \
        FUNC(COMMA, 0x33, __VA_ARGS__) \
        FUNC(PERIOD, 0x34, __VA_ARGS__) \
        FUNC(SLASH, 0x35, __VA_ARGS__) \
        FUNC(SHIFTRIGHT, 0x36, __VA_ARGS__) \
        FUNC(NUMPADMULTIPLY, 0x37, __VA_ARGS__) \
        FUNC(ALTLEFT, 0x38, __VA_ARGS__) \
        FUNC(SPACEBAR, 0x39, __VA_ARGS__) \
        FUNC(CAPSLOCK, 0x3A, __VA_ARGS__) \
        FUNC(ENUM_SIZE, 59, __VA_ARGS__)

    BB_CREATE_ENUM(KEYBOARD_KEY, uint32_t, KEYBOARD_KEY_D)

	struct MouseInfo
	{
		float2 move_offset;
        float2 mouse_pos;
		//Might add more here.
		int16_t wheel_move;
		bool left_pressed;
		bool left_released;
		bool right_pressed;
		bool right_released;
		bool middle_pressed;
		bool middle_released;
	};

	//7 byte struct (assuming bool is 1 byte.)
	struct KeyInfo
	{
		KEYBOARD_KEY scan_code;
		wchar utf16; //NOT IN USE;
		bool key_pressed;
	};

	enum class INPUT_TYPE : int32_t
	{
		MOUSE,
		KEYBOARD
	};

	struct InputEvent
	{
		INPUT_TYPE input_type;
		union
		{
			MouseInfo mouse_info{};
			KeyInfo key_info;
		};
	};

	void PollInputEvents(InputEvent* a_event_buffers, size_t& input_event_amount);
}
