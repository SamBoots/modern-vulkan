//entry for the vulkan renderer
#include "BBMain.h"
#include "BBMemory.h"
#include "Program.h"
#include "BBThreadScheduler.hpp"
#include "HID.h"

#include <chrono>

#include "shared_common.hlsl.h"

#include "Math.inl"
#include "BBjson.hpp"
#include "Editor.hpp"
#include "GameMain.hpp"
#include "RenderViewport.hpp"

#include "EngineConfig.hpp"

#include "Profiler.hpp"

using namespace BB;

static EngineConfig engine_config;
static bool end_app = false;
static bool resize_app = false;

static void CustomCloseWindow(const BB::WindowHandle a_window_handle)
{
	(void)a_window_handle;
	WriteEngineConfigData(engine_config);
	end_app = true;
}

static void CustomResizeWindow(const BB::WindowHandle a_window_handle, const uint32_t, const uint32_t)
{
	(void)a_window_handle;
	resize_app = true;
}

static void CustomMoveWindow(const BB::WindowHandle a_window_handle, const uint32_t a_x, const uint32_t a_y)
{
	(void)a_window_handle;
	engine_config.window_offset_x = a_x;
	engine_config.window_offset_y = a_y;
}

int main(int argc, char** argv)
{
	(void)argc;

	StackString<512> exe_path{};

	{
		const StringView exe_path_manipulator{ argv[0] };
		const size_t path_end = exe_path_manipulator.find_last_of('\\');

		exe_path.append(exe_path_manipulator.c_str(), path_end);
	}
	
	BBInitInfo bb_init{};
	bb_init.exe_path = exe_path.c_str();
	bb_init.program_name = L"Modern Vulkan - Editor";
	InitBB(bb_init);

	SystemInfo sys_info;
	OSSystemInfo(sys_info);
	Threads::InitThreads(sys_info.processor_num / 2);

	MemoryArena main_arena = MemoryArenaCreate();


	MemoryArenaScope(main_arena)
	{
		GetEngineConfigData(main_arena, engine_config);
	}

	InitializeProfiler(main_arena, 64);
	BB_START_PROFILE("startup profile");

	const uint2 window_extent = uint2(engine_config.window_size_x, engine_config.window_size_y);
	const uint2 window_offest = uint2(engine_config.window_offset_x, engine_config.window_offset_y);

	const WindowHandle window_handle = CreateOSWindow(
		BB::OS_WINDOW_STYLE::MAIN,
		static_cast<int>(window_offest.x),
		static_cast<int>(window_offest.y),
		static_cast<int>(window_extent.x),
		static_cast<int>(window_extent.y),
		L"Modern Vulkan - editor");

	RendererCreateInfo render_create_info;
	render_create_info.app_name = "modern vulkan - editor";
	render_create_info.engine_name = "building block engine - editor";
	render_create_info.window_handle = window_handle;
	render_create_info.swapchain_width = window_extent.x;
	render_create_info.swapchain_height = window_extent.y;
	render_create_info.gamma = 2.2f;
	render_create_info.debug = true;

	InitializeRenderer(main_arena, render_create_info);
	const uint32_t back_buffer_count = GetRenderIO().frame_count;

	const Asset::AssetManagerInitInfo asset_manager_info = {};
	Asset::InitializeAssetManager(main_arena, asset_manager_info);

	Editor editor{};
	editor.Init(main_arena, window_handle, window_extent);

	SetWindowCloseEvent(CustomCloseWindow);
	SetWindowResizeEvent(CustomResizeWindow);
	SetWindowMoveEvent(CustomMoveWindow);

	auto current_time = std::chrono::high_resolution_clock::now();

	float delta_time = 0;

	DungeonGame def_game{};
	def_game.Init(window_extent / 2, back_buffer_count);

	RenderViewport render_viewport{};
	render_viewport.Init(window_extent / 2, back_buffer_count, "../../resources/scenes/standard_scene.json");

	BB_END_PROFILE("startup profile");

	while (!end_app)
	{
		if (resize_app)
		{
			int x, y;
			OSGetWindowSize(window_handle, x, y);
			const uint2 new_extent = uint2(static_cast<uint32_t>(x), static_cast<uint32_t>(y));
			editor.ResizeWindow(new_extent);
			resize_app = false;
		}

		BB_START_PROFILE("frame time");
		Asset::Update();

		InputEvent input_events[INPUT_EVENT_BUFFER_MAX]{};
		size_t input_event_count = 0;

		ProcessMessages(window_handle);
		PollInputEvents(input_events, input_event_count);
		editor.StartFrame(main_arena, Slice(input_events, input_event_count), delta_time);
		const ThreadTask tasks[2]
		{
			editor.UpdateViewport(main_arena, delta_time, render_viewport, Slice(input_events, input_event_count)),
			editor.UpdateViewport(main_arena, delta_time, def_game, Slice(input_events, input_event_count))
		};

		for (size_t i = 0; i < _countof(tasks); i++)
		{
			Threads::WaitForTask(tasks[i]);
		}

		editor.EndFrame(main_arena);
		auto current_new = std::chrono::high_resolution_clock::now();
		delta_time = std::chrono::duration<float, std::chrono::seconds::period>(current_new - current_time).count();

		current_time = current_new;
		BB_END_PROFILE("frame time");
	}

	editor.Destroy();

	return 0;
}
