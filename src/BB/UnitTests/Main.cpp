// main.cpp : This file contains the 'main' function. Program execution begins and ends there.

#pragma warning(push)
#pragma warning(disable:26495)
#pragma warning(disable:26439)
#pragma warning(disable:26812)
#include <gtest/gtest.h>
#pragma warning(pop)

#include "BBMain.h"
#include "OS/Program.h"
#include "OS/HID.h"
#include "BBThreadScheduler.hpp"

using namespace BB;
int main(int argc, char** argv)
{
	BBInitInfo t_BBInitInfo;
	t_BBInitInfo.exe_path = argv[0];
	t_BBInitInfo.program_name = L"BB_UNIT_TEST";
	InitBB(t_BBInitInfo);

	Threads::InitThreads(8);

	Logger::EnableLogTypes(0);
	Logger::EnableLogType(WarningType::INFO);
	testing::InitGoogleTest();
	RUN_ALL_TESTS();
	WindowHandle mainWindow = CreateOSWindow(OS_WINDOW_STYLE::MAIN, 250, 200, 250, 200, L"Unit Test Main Window");
	Logger::EnableLogTypes(UINT32_MAX);

	bool hasWindows = true;
	InputEvent t_InputEvents[INPUT_EVENT_BUFFER_MAX]{};
	size_t t_InputEventCount = 0;
	while (hasWindows)
	{
		BB::PollInputEvents(t_InputEvents, t_InputEventCount);
		for (size_t i = 0; i < t_InputEventCount; i++)
		{
			InputEvent& event = t_InputEvents[i];
			if (event.input_type == INPUT_TYPE::KEYBOARD)
			{
				switch (event.key_info.scan_code)
				{
				case KEYBOARD_KEY::W:
					if (event.key_info.key_pressed)
						BB_LOG("W pressed!");
					else
						BB_LOG("W released!");
					break;
				case KEYBOARD_KEY::A:
					if (event.key_info.key_pressed)
						BB_LOG("A pressed!");
					else
						BB_LOG("A released!");
					break;
				case KEYBOARD_KEY::S:
					if (event.key_info.key_pressed)
						BB_LOG("S pressed!");
					else
						BB_LOG("S released!");
					break;
				case KEYBOARD_KEY::D:
					if (event.key_info.key_pressed)
						BB_LOG("D pressed!");
					else
						BB_LOG("D released!");
					break;
				}
			}
		}
		hasWindows = ProcessMessages(mainWindow);
	}

	return 0;
}

//Suppress the stack warning, is safe since this code is only used for the unit testing here.
//not sure if this really destroys anything but for the purposes of testing it seems to work.
#pragma warning(disable:6262)
#include "Framework/Allocators_UTEST.h"
#include "Framework/Array_UTEST.h"
#include "Framework/Pool_UTEST.h"
#include "Framework/Hashmap_UTEST.h"
#include "Framework/MemoryArena_UTEST.h"
#include "Framework/BBjson_UTEST.hpp"
#include "Framework/MemoryOperations_UTEST.h"
#include "Framework/Slice_UTEST.h"
#include "Framework/Slotmap_UTEST.h"
#include "Framework/String_UTEST.h"
#include "Framework/FileReadWrite_UTEST.h"
#pragma warning(default:6262)