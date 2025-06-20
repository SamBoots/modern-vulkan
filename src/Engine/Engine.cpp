#include "Engine.hpp"
#include "Utils/Logger.h"
#include "BBMain.h"
#include "BBThreadScheduler.hpp"
#include "OS/Program.h"

#include "EngineConfig.hpp"
#include "MaterialSystem.hpp"
#include "Profiler.hpp"
#include "Renderer.hpp"
#include "AssetLoader.hpp"
#include "InputSystem.hpp"

#include "lua/LuaTest.hpp"

#include "BBGlobal.h"

using namespace BB;

static EngineConfig engine_config;
static PathString s_root_path;
static bool s_window_closed = false;
static bool s_resize_app = false;

static void CustomCloseWindow(const BB::WindowHandle a_window_handle)
{
    (void)a_window_handle;
    WriteEngineConfigData(engine_config);
    s_window_closed = true;
}

static void CustomResizeWindow(const BB::WindowHandle a_window_handle, const uint32_t a_x, const uint32_t a_y)
{
    (void)a_window_handle;
    engine_config.window_size_x = a_x;
    engine_config.window_size_y = a_y;
    s_resize_app = true;
}

static void CustomMoveWindow(const BB::WindowHandle a_window_handle, const uint32_t a_x, const uint32_t a_y)
{
    (void)a_window_handle;
    engine_config.window_offset_x = a_x;
    engine_config.window_offset_y = a_y;
}

EngineInfo BB::InitEngine(MemoryArena& a_arena, const wchar* a_app_name, const EngineOptions& a_engine_options, const GraphicOptions& a_graphic_options)
{
    StackString<512> exe_path{};

    const StringView exe_path_manipulator{ a_engine_options.exe_path };
    const size_t path_end = exe_path_manipulator.find_last_of('\\');

    exe_path.append(exe_path_manipulator.c_str(), path_end);

    BBInitInfo bb_init{};
    bb_init.exe_path = exe_path.c_str();
    bb_init.program_name = a_app_name;
    InitBB(bb_init);

    // really improve on this directory shit
    const size_t first_slash = exe_path.find_last_of_directory_slash();
    const size_t src_slash = exe_path.GetView(first_slash).find_last_of_directory_slash();
    s_root_path = PathString(exe_path.c_str(), src_slash);

    SystemInfo sys_info;
    OSSystemInfo(sys_info);
    Threads::InitThreads(sys_info.processor_num / 2);

    MemoryArenaScope(a_arena)
    {
        GetEngineConfigData(a_arena, engine_config);
    }

    const uint2 window_extent = uint2(engine_config.window_size_x, engine_config.window_size_y);
    const uint2 window_offest = uint2(engine_config.window_offset_x, engine_config.window_offset_y);

    const WindowHandle window_handle = CreateOSWindow(
        BB::OS_WINDOW_STYLE::MAIN,
        static_cast<int>(window_offest.x),
        static_cast<int>(window_offest.y),
        static_cast<int>(window_extent.x),
        static_cast<int>(window_extent.y),
        a_app_name);

    SetWindowCloseEvent(CustomCloseWindow);
    SetWindowResizeEvent(CustomResizeWindow);
    SetWindowMoveEvent(CustomMoveWindow);

    RendererCreateInfo render_create_info;
    // TEMP name
    render_create_info.app_name = "modern vulkan - editor";
    render_create_info.engine_name = "building block engine";
    render_create_info.window_handle = window_handle;
    render_create_info.swapchain_width = window_extent.x;
    render_create_info.swapchain_height = window_extent.y;
    render_create_info.gamma = 2.2f;
    render_create_info.debug = a_engine_options.enable_debug;
    render_create_info.use_raytracing = a_graphic_options.use_raytracing;

    InitializeRenderer(a_arena, render_create_info);
    const uint32_t back_buffer_count = GetBackBufferCount();

    MaterialSystemCreateInfo material_system_init{};
    material_system_init.max_materials = 128;
    material_system_init.max_shader_effects = 64;
    material_system_init.max_material_instances = 256;
    material_system_init.default_2d_vertex.path = "../../resources/shaders/hlsl/Imgui.hlsl";
    material_system_init.default_2d_vertex.entry = "VertexMain";
    material_system_init.default_2d_vertex.stage = SHADER_STAGE::VERTEX;
    material_system_init.default_2d_vertex.next_stages = static_cast<SHADER_STAGE_FLAGS>(SHADER_STAGE::FRAGMENT_PIXEL);

    material_system_init.default_2d_fragment.path = "../../resources/shaders/hlsl/Imgui.hlsl";
    material_system_init.default_2d_fragment.entry = "FragmentMain";
    material_system_init.default_2d_fragment.stage = SHADER_STAGE::FRAGMENT_PIXEL;
    material_system_init.default_2d_fragment.next_stages = static_cast<SHADER_STAGE_FLAGS>(SHADER_STAGE::NONE);

    material_system_init.default_3d_vertex.path = "../../resources/shaders/hlsl/pbrmesh.hlsl";
    material_system_init.default_3d_vertex.entry = "VertexMain";
    material_system_init.default_3d_vertex.stage = SHADER_STAGE::VERTEX;
    material_system_init.default_3d_vertex.next_stages = static_cast<SHADER_STAGE_FLAGS>(SHADER_STAGE::FRAGMENT_PIXEL);

    material_system_init.default_3d_fragment.path = "../../resources/shaders/hlsl/pbrmesh.hlsl";
    material_system_init.default_3d_fragment.entry = "FragmentMain";
    material_system_init.default_3d_fragment.stage = SHADER_STAGE::FRAGMENT_PIXEL;
    material_system_init.default_3d_fragment.next_stages = static_cast<SHADER_STAGE_FLAGS>(SHADER_STAGE::NONE);

    Material::InitMaterialSystem(a_arena, material_system_init);

    const Asset::AssetManagerInitInfo asset_manager_info = {};
    Asset::InitializeAssetManager(a_arena, asset_manager_info);

    Input::InitInputSystem(a_arena);

    if (a_engine_options.enable_debug)
    {
        InitializeProfiler(a_arena, a_engine_options.debug_options.max_profiler_entries);
    }


    EngineInfo info;
    info.window_handle = window_handle;
    info.window_extent = window_extent;
    info.backbuffer_count = back_buffer_count;

    bool validate_lua = lua_RunBBTypeTest();
    BB_ASSERT(validate_lua, "lua validation failed");

    return info;
}

bool BB::DestroyEngine()
{
    BB_UNIMPLEMENTED("destroy engine");
    return true;
}

ENGINE_STATUS BB::UpdateEngine(const WindowHandle a_window_handle, const ConstSlice<InputEvent> a_input_events)
{
    if (s_window_closed)
        return ENGINE_STATUS::CLOSE_APP;
    ENGINE_STATUS status = ENGINE_STATUS::RESUME;
    if (s_resize_app)
    {
        s_resize_app = false;
        status = ENGINE_STATUS::RESIZE;
    }
    Asset::Update();

    Input::UpdateInput(a_input_events);
    return status;
}

const char* BB::GetExePath()
{
    return g_exe_path;
}

const StringView BB::GetRootPath()
{
    return s_root_path.GetView();
}
