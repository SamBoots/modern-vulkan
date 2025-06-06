#pragma once
#include "Common.h"

namespace BB
{
	constexpr size_t INPUT_EVENT_BUFFER_MAX = 64;

	//These will be translated, the are already close to their real counterpart.
	//Translation table: https://download.microsoft.com/download/1/6/1/161ba512-40e2-4cc9-843a-923143f3456c/translate.pdf
	enum class KEYBOARD_KEY : uint32_t
	{
		NOKEY = 0x00,
		ESCAPE = 0x01,
		KEY_1 = 0x02,
		KEY_2 = 0x03,
		KEY_3 = 0x04,
		KEY_4 = 0x05,
		KEY_5 = 0x06,
		KEY_6 = 0x07,
		KEY_7 = 0x08,
		KEY_8 = 0x09,
		KEY_9 = 0x0A,
		KEY_0 = 0x0B,
		MINUS = 0x0C,
		EQUALS = 0x0D,
		BACKSPACE = 0x0E,
		TAB = 0x0F,
		Q = 0x10,
		W = 0x11,
		E = 0x12,
		R = 0x13,
		T = 0x14,
		Y = 0x15,
		U = 0x16,
		I = 0x17,
		O = 0x18,
		P = 0x19,
		BRACKETLEFT = 0x1A,
		BRACKETRIGHT = 0x1B,
		RETURN = 0x1C, //I think this is non-numpad enter?
		CONTROLLEFT = 0x1D,
		A = 0x1E,
		S = 0x1F,
		D = 0x20,
		F = 0x21,
		G = 0x22,
		H = 0x23,
		J = 0x24,
		K = 0x25,
		L = 0x26,
		SEMICOLON = 0x27,
		APOSTROPHE = 0x28,
		GRAVE = 0x29,
		SHIFTLEFT = 0x2A,
		BACKSLASH = 0x2B,
		Z = 0x2C,
		X = 0x2D,
		C = 0x2E,
		V = 0x2F,
		B = 0x30,
		N = 0x31,
		M = 0x32,
		COMMA = 0x33,
		PERIOD = 0x34,
		SLASH = 0x35,
		SHIFTRIGHT = 0x36,
		NUMPADMULTIPLY = 0x37,
		ALTLEFT = 0x38,
		SPACEBAR = 0x39,
		CAPSLOCK = 0x3A,

        ENUM_SIZE,
	};

    static inline const char* KEYBOARD_KEY_STR(const KEYBOARD_KEY a_key)
    {
        switch (a_key)
        {
            ENUM_CASE_STR(KEYBOARD_KEY::NOKEY);
            ENUM_CASE_STR(KEYBOARD_KEY::ESCAPE);
            ENUM_CASE_STR(KEYBOARD_KEY::KEY_1);
            ENUM_CASE_STR(KEYBOARD_KEY::KEY_2);
            ENUM_CASE_STR(KEYBOARD_KEY::KEY_3);
            ENUM_CASE_STR(KEYBOARD_KEY::KEY_4);
            ENUM_CASE_STR(KEYBOARD_KEY::KEY_5);
            ENUM_CASE_STR(KEYBOARD_KEY::KEY_6);
            ENUM_CASE_STR(KEYBOARD_KEY::KEY_7);
            ENUM_CASE_STR(KEYBOARD_KEY::KEY_8);
            ENUM_CASE_STR(KEYBOARD_KEY::KEY_9);
            ENUM_CASE_STR(KEYBOARD_KEY::KEY_0);
            ENUM_CASE_STR(KEYBOARD_KEY::MINUS);
            ENUM_CASE_STR(KEYBOARD_KEY::EQUALS);
            ENUM_CASE_STR(KEYBOARD_KEY::BACKSPACE);
            ENUM_CASE_STR(KEYBOARD_KEY::TAB);
            ENUM_CASE_STR(KEYBOARD_KEY::Q);
            ENUM_CASE_STR(KEYBOARD_KEY::W);
            ENUM_CASE_STR(KEYBOARD_KEY::E);
            ENUM_CASE_STR(KEYBOARD_KEY::R);
            ENUM_CASE_STR(KEYBOARD_KEY::T);
            ENUM_CASE_STR(KEYBOARD_KEY::Y);
            ENUM_CASE_STR(KEYBOARD_KEY::U);
            ENUM_CASE_STR(KEYBOARD_KEY::I);
            ENUM_CASE_STR(KEYBOARD_KEY::O);
            ENUM_CASE_STR(KEYBOARD_KEY::P);
            ENUM_CASE_STR(KEYBOARD_KEY::BRACKETLEFT);
            ENUM_CASE_STR(KEYBOARD_KEY::BRACKETRIGHT);
            ENUM_CASE_STR(KEYBOARD_KEY::RETURN); //I think this is non-numpad enter?
            ENUM_CASE_STR(KEYBOARD_KEY::CONTROLLEFT);
            ENUM_CASE_STR(KEYBOARD_KEY::A);
            ENUM_CASE_STR(KEYBOARD_KEY::S);
            ENUM_CASE_STR(KEYBOARD_KEY::D);
            ENUM_CASE_STR(KEYBOARD_KEY::F);
            ENUM_CASE_STR(KEYBOARD_KEY::G);
            ENUM_CASE_STR(KEYBOARD_KEY::H);
            ENUM_CASE_STR(KEYBOARD_KEY::J);
            ENUM_CASE_STR(KEYBOARD_KEY::K);
            ENUM_CASE_STR(KEYBOARD_KEY::L);
            ENUM_CASE_STR(KEYBOARD_KEY::SEMICOLON);
            ENUM_CASE_STR(KEYBOARD_KEY::APOSTROPHE);
            ENUM_CASE_STR(KEYBOARD_KEY::GRAVE);
            ENUM_CASE_STR(KEYBOARD_KEY::SHIFTLEFT);
            ENUM_CASE_STR(KEYBOARD_KEY::BACKSLASH);
            ENUM_CASE_STR(KEYBOARD_KEY::Z);
            ENUM_CASE_STR(KEYBOARD_KEY::X);
            ENUM_CASE_STR(KEYBOARD_KEY::C);
            ENUM_CASE_STR(KEYBOARD_KEY::V);
            ENUM_CASE_STR(KEYBOARD_KEY::B);
            ENUM_CASE_STR(KEYBOARD_KEY::N);
            ENUM_CASE_STR(KEYBOARD_KEY::M);
            ENUM_CASE_STR(KEYBOARD_KEY::COMMA);
            ENUM_CASE_STR(KEYBOARD_KEY::PERIOD);
            ENUM_CASE_STR(KEYBOARD_KEY::SLASH);
            ENUM_CASE_STR(KEYBOARD_KEY::SHIFTRIGHT);
            ENUM_CASE_STR(KEYBOARD_KEY::NUMPADMULTIPLY);
            ENUM_CASE_STR(KEYBOARD_KEY::ALTLEFT);
            ENUM_CASE_STR(KEYBOARD_KEY::SPACEBAR);
            ENUM_CASE_STR(KEYBOARD_KEY::CAPSLOCK);
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
