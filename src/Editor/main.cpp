//entry for the vulkan renderer
#include "Program.h"
#include "BBThreadScheduler.hpp"
#include "HID.h"

#include "Engine.hpp"

#include <chrono>

#include "shared_common.hlsl.h"

#include "Math/Math.inl"
#include "BBjson.hpp"
#include "Editor.hpp"
#include "GameMain.hpp"
#include "GameInstance.hpp"

#include "Profiler.hpp"

using namespace BB;

int main(int argc, char** argv)
{
	(void)argc;

    MemoryArena main_arena = MemoryArenaCreate();
    EngineInfo engine_info;
    {
        EngineOptions engine_options;
        engine_options.exe_path = argv[0];
        engine_options.max_materials = 128;
        engine_options.max_shader_effects = 64;
        engine_options.max_material_instances = 256;
        engine_options.enable_debug = true;
        engine_options.debug_options.max_profiler_entries = 64;

        GraphicOptions graphic_options;
        graphic_options.use_raytracing = true; // error when false, checkout
        engine_info = InitEngine(main_arena, L"Modern Vulkan - Editor", engine_options, graphic_options);
    }

	Editor editor{};
	editor.Init(main_arena, engine_info.window_handle, engine_info.window_extent);

	auto current_time = std::chrono::high_resolution_clock::now();

	float delta_time = 0;
    
	//DungeonGame def_game{};
	//def_game.Init(engine_info.window_extent / 2, engine_info.backbuffer_count, "dungeon");

    GameInstance render_viewport{};
    render_viewport.Init(engine_info.window_extent / 2, "rendershowcase", nullptr);
     
	while (true)
	{
        InputEvent input_events[INPUT_EVENT_BUFFER_MAX]{};
        size_t input_event_count = 0;

        ProcessMessages(engine_info.window_handle);
        PollInputEvents(input_events, input_event_count);
        const ENGINE_STATUS status = UpdateEngine(engine_info.window_handle, ConstSlice<InputEvent>(input_events, input_event_count));

        if (status == ENGINE_STATUS::CLOSE_APP)
            break;

		if (status == ENGINE_STATUS::RESIZE)
		{
			int x, y;
			OSGetWindowSize(engine_info.window_handle, x, y);
			const uint2 new_extent = uint2(static_cast<uint32_t>(x), static_cast<uint32_t>(y));
			editor.ResizeWindow(new_extent);
		}

		BB_START_PROFILE("frame time");


		editor.StartFrame(main_arena, Slice(input_events, input_event_count), delta_time);

		const ThreadTask tasks[1]
		{
			editor.UpdateViewport(main_arena, delta_time, render_viewport)
			//editor.UpdateViewport(main_arena, delta_time, def_game)
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
