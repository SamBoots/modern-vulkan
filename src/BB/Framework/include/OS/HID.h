#pragma once
#include "Common.h"

namespace BB
{
	constexpr size_t INPUT_EVENT_BUFFER_MAX = 64;

	//These will be translated, the are already close to their real counterpart.
	//Translation table: https://download.microsoft.com/download/1/6/1/161ba512-40e2-4cc9-843a-923143f3456c/translate.pdf
#define KEYBOARD_KEY_D(FUNC) \
		FUNC(NOKEY, 0x00) \
        FUNC(ESCAPE, 0x01) \
        FUNC(KEY_1, 0x02) \
        FUNC(KEY_2, 0x03) \
        FUNC(KEY_3, 0x04) \
        FUNC(KEY_4, 0x05) \
        FUNC(KEY_5, 0x06) \
        FUNC(KEY_6, 0x07) \
        FUNC(KEY_7, 0x08) \
        FUNC(KEY_8, 0x09) \
        FUNC(KEY_9, 0x0A) \
        FUNC(KEY_0, 0x0B) \
        FUNC(MINUS, 0x0C) \
        FUNC(EQUALS, 0x0D) \
        FUNC(BACKSPACE, 0x0E) \
        FUNC(TAB, 0x0F) \
        FUNC(Q, 0x10) \
        FUNC(W, 0x11) \
        FUNC(E, 0x12) \
        FUNC(R, 0x13) \
        FUNC(T, 0x14) \
        FUNC(Y, 0x15) \
        FUNC(U, 0x16) \
        FUNC(I, 0x17) \
        FUNC(O, 0x18) \
        FUNC(P, 0x19) \
        FUNC(BRACKETLEFT, 0x1A) \
        FUNC(BRACKETRIGHT, 0x1B) \
        /* I think this is non-numpad enter? */ \
        FUNC(RETURN, 0x1C) \
        FUNC(CONTROLLEFT, 0x1D) \
        FUNC(A, 0x1E) \
        FUNC(S, 0x1F) \
        FUNC(D, 0x20) \
        FUNC(F, 0x21) \
        FUNC(G, 0x22) \
        FUNC(H, 0x23) \
        FUNC(J, 0x24) \
        FUNC(K, 0x25) \
        FUNC(L, 0x26) \
        FUNC(SEMICOLON, 0x27) \
        FUNC(APOSTROPHE, 0x28) \
        FUNC(GRAVE, 0x29) \
        FUNC(SHIFTLEFT, 0x2A) \
        FUNC(BACKSLASH, 0x2B) \
        FUNC(Z, 0x2C) \
        FUNC(X, 0x2D) \
        FUNC(C, 0x2E) \
        FUNC(V, 0x2F) \
        FUNC(B, 0x30) \
        FUNC(N, 0x31) \
        FUNC(M, 0x32) \
        FUNC(COMMA, 0x33) \
        FUNC(PERIOD, 0x34) \
        FUNC(SLASH, 0x35) \
        FUNC(SHIFTRIGHT, 0x36) \
        FUNC(NUMPADMULTIPLY, 0x37) \
        FUNC(ALTLEFT, 0x38) \
        FUNC(SPACEBAR, 0x39) \
        FUNC(CAPSLOCK, 0x3A)

	enum class KEYBOARD_KEY : uint32_t
	{
        #define KEY_ENUM_ENTRY(name, value) name = value,
            KEYBOARD_KEY_D(KEY_ENUM_ENTRY)
        #undef KEY_ENUM_ENTRY

        ENUM_SIZE
	};

    static inline const char* KEYBOARD_KEY_STR(const KEYBOARD_KEY a_key)
    {
        switch (a_key)
        {
            #define KEYBOARD_KEY_SWITCH(name, value) ENUM_CASE_STR(KEYBOARD_KEY, name);
                KEYBOARD_KEY_D(KEYBOARD_KEY_SWITCH)
            #undef KEYBOARD_KEY_SWITCH
            ENUM_CASE_STR_NOT_FOUND();
        }
    };

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
