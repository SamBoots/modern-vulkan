#pragma once
#include "Common.h"

namespace BB
{
	constexpr size_t INPUT_EVENT_BUFFER_MAX = 64;

	//These will be translated, the are already close to their real counterpart.
	//Translation table: https://download.microsoft.com/download/1/6/1/161ba512-40e2-4cc9-843a-923143f3456c/translate.pdf
#define KEYBOARD_KEY_D(FUNCT) \
		FUNCT(NOKEY, 0x00) \
        FUNCT(ESCAPE, 0x01) \
        FUNCT(KEY_1, 0x02) \
        FUNCT(KEY_2, 0x03) \
        FUNCT(KEY_3, 0x04) \
        FUNCT(KEY_4, 0x05) \
        FUNCT(KEY_5, 0x06) \
        FUNCT(KEY_6, 0x07) \
        FUNCT(KEY_7, 0x08) \
        FUNCT(KEY_8, 0x09) \
        FUNCT(KEY_9, 0x0A) \
        FUNCT(KEY_0, 0x0B) \
        FUNCT(MINUS, 0x0C) \
        FUNCT(EQUALS, 0x0D) \
        FUNCT(BACKSPACE, 0x0E) \
        FUNCT(TAB, 0x0F) \
        FUNCT(Q, 0x10) \
        FUNCT(W, 0x11) \
        FUNCT(E, 0x12) \
        FUNCT(R, 0x13) \
        FUNCT(T, 0x14) \
        FUNCT(Y, 0x15) \
        FUNCT(U, 0x16) \
        FUNCT(I, 0x17) \
        FUNCT(O, 0x18) \
        FUNCT(P, 0x19) \
        FUNCT(BRACKETLEFT, 0x1A) \
        FUNCT(BRACKETRIGHT, 0x1B) \
        /* I think this is non-numpad enter? */ \
        FUNCT(RETURN, 0x1C) \
        FUNCT(CONTROLLEFT, 0x1D) \
        FUNCT(A, 0x1E) \
        FUNCT(S, 0x1F) \
        FUNCT(D, 0x20) \
        FUNCT(F, 0x21) \
        FUNCT(G, 0x22) \
        FUNCT(H, 0x23) \
        FUNCT(J, 0x24) \
        FUNCT(K, 0x25) \
        FUNCT(L, 0x26) \
        FUNCT(SEMICOLON, 0x27) \
        FUNCT(APOSTROPHE, 0x28) \
        FUNCT(GRAVE, 0x29) \
        FUNCT(SHIFTLEFT, 0x2A) \
        FUNCT(BACKSLASH, 0x2B) \
        FUNCT(Z, 0x2C) \
        FUNCT(X, 0x2D) \
        FUNCT(C, 0x2E) \
        FUNCT(V, 0x2F) \
        FUNCT(B, 0x30) \
        FUNCT(N, 0x31) \
        FUNCT(M, 0x32) \
        FUNCT(COMMA, 0x33) \
        FUNCT(PERIOD, 0x34) \
        FUNCT(SLASH, 0x35) \
        FUNCT(SHIFTRIGHT, 0x36) \
        FUNCT(NUMPADMULTIPLY, 0x37) \
        FUNCT(ALTLEFT, 0x38) \
        FUNCT(SPACEBAR, 0x39) \
        FUNCT(CAPSLOCK, 0x3A)

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
