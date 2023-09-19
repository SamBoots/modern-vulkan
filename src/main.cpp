//entry for the vulkan renderer
#include "BBMain.h"
#include "BBMemory.h"
#include "Program.h"
#include "BBThreadScheduler.hpp"
#include "HID.h"

int main(int argc, char** argv)
{
	using namespace BB;

	BBInitInfo bb_init;
	bb_init.exePath = argv[0];
	bb_init.programName = L"Modern Vulkan";
	InitBB(bb_init);

	SystemInfo sys_info;
	OSSystemInfo(sys_info);
	Threads::InitThreads(sys_info.processor_num);

	StackAllocator_t main_allocator{ mbSize * 6 };

	int window_width = 1280;
	int window_height = 720;
	WindowHandle window = BB::CreateOSWindow(
		BB::OS_WINDOW_STYLE::MAIN,
		250,
		200,
		window_width,
		window_height,
		L"CrossRenderer");

	InputEvent input_events[INPUT_EVENT_BUFFER_MAX];
	size_t input_event_count = 0;

	bool quit_app = false;
	while (!quit_app)
	{
		ProcessMessages(window);
		PollInputEvents(input_events, input_event_count);

		for (size_t i = 0; i < input_event_count; i++)
		{
			const InputEvent& ip = input_events[i];

			if (ip.inputType == INPUT_TYPE::KEYBOARD)
			{
				const KeyInfo& ki = ip.keyInfo;
			}
			else if (ip.inputType == INPUT_TYPE::MOUSE)
			{
				const MouseInfo& mi = ip.mouseInfo;
			}
		}
	}

	return 0;
}