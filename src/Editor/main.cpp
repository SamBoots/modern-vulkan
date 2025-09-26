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
#include "EditorGame.hpp"

#include "Profiler.hpp"

#include "DungeonGameLib.hpp"

using namespace BB;

int main(int argc, char** argv)
{
	(void)argc;
    (void)argv;

    MemoryArena main_arena = MemoryArenaCreate();
    EngineInfo engine_info;
    {
        EngineOptions engine_options;
        // editor should give the src path 
        //engine_options.exe_path = argv[0];
        engine_options.exe_path = EDITOR_SRC_PATH;
        engine_options.max_materials = 128;
        engine_options.max_shader_effects = 64;
        engine_options.max_material_instances = 256;
        engine_options.enable_debug = true;
        engine_options.debug_options.max_profiler_entries = 64;

        GraphicOptions graphic_options;
        graphic_options.use_raytracing = true; // warnings when false, checkout
        engine_info = InitEngine(main_arena, L"Modern Vulkan - Editor", engine_options, graphic_options);
    }

	Editor editor{};
    {
        FixedArray<PFN_LuaPluginRegisterFunctions, 1> lua_plugins;
        lua_plugins[0] = RegisterDungeonGameLibLuaFunctions;
        FixedArray<EditorGameCreateInfo, 2> game_infos;
        game_infos[0].dir_name = "rendershowcase";
        game_infos[0].register_funcs = {};
        game_infos[1].dir_name = "dungeon";
        game_infos[1].register_funcs = lua_plugins.const_slice();
        EditorCreateInfo create_info;
        create_info.window = engine_info.window_handle;
        create_info.window_extent = engine_info.window_extent;
        create_info.game_instance_max = 8;
        create_info.initial_games = game_infos.const_slice();

        editor.Init(main_arena, create_info);
    }

	auto current_time = std::chrono::high_resolution_clock::now();

	float delta_time = 0;

    bool end_app = false;
	while (end_app == false)
	{
        InputEvent input_events[INPUT_EVENT_BUFFER_MAX]{};
        size_t input_event_count = 0;

        ProcessMessages(engine_info.window_handle);
        PollInputEvents(input_events, input_event_count);
        const ENGINE_STATUS status = UpdateEngine(ConstSlice<InputEvent>(input_events, input_event_count));

        if (status == ENGINE_STATUS::CLOSE_APP)
            end_app = true;

		if (status == ENGINE_STATUS::RESIZE)
		{
			int x, y;
			OSGetWindowSize(engine_info.window_handle, x, y);
			const uint2 new_extent = uint2(static_cast<uint32_t>(x), static_cast<uint32_t>(y));
			editor.ResizeWindow(new_extent);
		}

		BB_START_PROFILE("frame time");

		editor.StartFrame(main_arena, Slice(input_events, input_event_count), delta_time);
        editor.UpdateGames(main_arena, delta_time);
		editor.EndFrame(main_arena);
		auto current_new = std::chrono::high_resolution_clock::now();
		delta_time = std::chrono::duration<float, std::chrono::seconds::period>(current_new - current_time).count();

		current_time = current_new;
		BB_END_PROFILE("frame time");
	}

	editor.Destroy();

	return 0;
}
