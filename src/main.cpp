//entry for the vulkan renderer
#include "BBMain.h"
#include "BBMemory.h"
#include "Program.h"
#include "BBThreadScheduler.hpp"
#include "HID.h"


#include "Renderer.hpp"
#include "Math.inl"

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

	StackAllocator_t main_allocator{ mbSize * 32 };

	int window_width = 1280;
	int window_height = 720;
	const WindowHandle window = CreateOSWindow(
		BB::OS_WINDOW_STYLE::MAIN,
		250,
		200,
		window_width,
		window_height,
		L"Modern Vulkan");

	RendererCreateInfo render_create_info;
	render_create_info.app_name = "modern vulkan";
	render_create_info.engine_name = "Building Block Engine";
	render_create_info.window_handle = window;
	render_create_info.swapchain_width = static_cast<uint32_t>(window_width);
	render_create_info.swapchain_height = static_cast<uint32_t>(window_height);
	render_create_info.debug = true;
	InitializeRenderer(main_allocator, render_create_info);

	MeshHandle quad_mesh;
	BBStackAllocatorScope(main_allocator)
	{
		//Do some simpel model loading and drawing.
		Vertex vertices[4];
		vertices[0] = { {-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f} };
		vertices[1] = { {0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f} };
		vertices[2] = { {0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f} };
		vertices[3] = { {-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 1.0f, 1.0f} };

		const uint32_t indices[] = { 0, 1, 2, 2, 3, 0 };

		CreateMeshInfo quad_create_info{};
		quad_create_info.vertices = Slice(vertices, _countof(vertices));
		quad_create_info.indices = Slice(indices, _countof(indices));
		quad_mesh = CreateMesh(quad_create_info);
	}

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


		
		BBStackAllocatorScope(main_allocator)
		{
			StartFrame();

			//draw stuff here!
			DrawMesh(quad_mesh, Mat4x4Identity());

			EndFrame();
		}

	}

	DirectDestroyOSWindow(window);

	return 0;
}