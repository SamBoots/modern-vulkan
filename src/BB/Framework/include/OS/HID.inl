#include "HID.h"
namespace BB
{
	//array size is last one of the KEYBOARD_KEY list.
	static KEYBOARD_KEY s_translate_key[0x3A + 1];

	static void SetupHIDTranslates()
	{
		s_translate_key[0x1E] = KEYBOARD_KEY::A;
		s_translate_key[0x30] = KEYBOARD_KEY::B;
		s_translate_key[0x2E] = KEYBOARD_KEY::C;
		s_translate_key[0x20] = KEYBOARD_KEY::D;
		s_translate_key[0x12] = KEYBOARD_KEY::E;
		s_translate_key[0x21] = KEYBOARD_KEY::F;
		s_translate_key[0x22] = KEYBOARD_KEY::G;
		s_translate_key[0x23] = KEYBOARD_KEY::H;
		s_translate_key[0x17] = KEYBOARD_KEY::I;
		s_translate_key[0x24] = KEYBOARD_KEY::J;
		s_translate_key[0x25] = KEYBOARD_KEY::K;
		s_translate_key[0x26] = KEYBOARD_KEY::L;
		s_translate_key[0x32] = KEYBOARD_KEY::M;
		s_translate_key[0x31] = KEYBOARD_KEY::N;
		s_translate_key[0x18] = KEYBOARD_KEY::O;
		s_translate_key[0x19] = KEYBOARD_KEY::P;
		s_translate_key[0x10] = KEYBOARD_KEY::Q;
		s_translate_key[0x13] = KEYBOARD_KEY::R;
		s_translate_key[0x1F] = KEYBOARD_KEY::S;
		s_translate_key[0x14] = KEYBOARD_KEY::T;
		s_translate_key[0x16] = KEYBOARD_KEY::U;
		s_translate_key[0x2F] = KEYBOARD_KEY::V;
		s_translate_key[0x11] = KEYBOARD_KEY::W;
		s_translate_key[0x2D] = KEYBOARD_KEY::X;
		s_translate_key[0x15] = KEYBOARD_KEY::Y;
		s_translate_key[0x2C] = KEYBOARD_KEY::Z;
		s_translate_key[0x02] = KEYBOARD_KEY::KEY_1;
		s_translate_key[0x03] = KEYBOARD_KEY::KEY_2;
		s_translate_key[0x04] = KEYBOARD_KEY::KEY_3;
		s_translate_key[0x05] = KEYBOARD_KEY::KEY_4;
		s_translate_key[0x06] = KEYBOARD_KEY::KEY_5;
		s_translate_key[0x07] = KEYBOARD_KEY::KEY_6;
		s_translate_key[0x08] = KEYBOARD_KEY::KEY_7;
		s_translate_key[0x09] = KEYBOARD_KEY::KEY_8;
		s_translate_key[0x0A] = KEYBOARD_KEY::KEY_9;
		s_translate_key[0x0B] = KEYBOARD_KEY::KEY_0;
		s_translate_key[0x1C] = KEYBOARD_KEY::RETURN;
		s_translate_key[0x01] = KEYBOARD_KEY::ESCAPE;
		s_translate_key[0x0E] = KEYBOARD_KEY::BACKSPACE;
		s_translate_key[0x0F] = KEYBOARD_KEY::TAB;
		s_translate_key[0x39] = KEYBOARD_KEY::SPACEBAR;
		s_translate_key[0x0C] = KEYBOARD_KEY::MINUS;
		s_translate_key[0x0D] = KEYBOARD_KEY::EQUALS;
		s_translate_key[0x1A] = KEYBOARD_KEY::BRACKETLEFT;
		s_translate_key[0x1B] = KEYBOARD_KEY::BRACKETRIGHT;
		s_translate_key[0x2B] = KEYBOARD_KEY::BACKSLASH;

		s_translate_key[0x27] = KEYBOARD_KEY::SEMICOLON;
		s_translate_key[0x28] = KEYBOARD_KEY::APOSTROPHE;
		s_translate_key[0x29] = KEYBOARD_KEY::GRAVE;
		s_translate_key[0x33] = KEYBOARD_KEY::COMMA;
		s_translate_key[0x34] = KEYBOARD_KEY::PERIOD;
		s_translate_key[0x35] = KEYBOARD_KEY::SLASH;
		s_translate_key[0x3A] = KEYBOARD_KEY::CAPSLOCK;

		//After this we have the F1-12 keys.
		//s_translate_key[0x] = KEYBOARD_KEY::_;
	}
}
