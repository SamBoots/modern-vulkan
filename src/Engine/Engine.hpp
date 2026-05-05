#pragma once
#include "Enginefwd.hpp"
#include "Rendererfwd.hpp"
#include "MemoryArena.hpp"
#include "Slice.h"
#include "HID.h"
#include "Storage/BBString.h"

namespace BB
{
    enum class OS_WINDOW_STYLE;

    struct GraphicOptions
    {
        bool use_raytracing;
    };

    struct DebugOptions
    {
        uint32_t max_profiler_entries;
    };

    struct EngineOptions
    {
        const char* exe_path;
        uint32_t max_materials;
        uint32_t max_shader_effects;
        uint32_t max_material_instances;
        bool enable_debug;
        DebugOptions debug_options;
    };

    struct SwapchainWindow
    {
        WindowHandle hwnd;
        RSwapchain swapchain;
        uint2 extent;
        int2 offset;
        uint32_t backbuffer_count;
        bool resize;
        bool window_closed;
    };

    struct EngineInfo
    {
        SwapchainWindow& main_window;
        uint2 window_extent;
        uint32_t backbuffer_count;
    };

    enum class ENGINE_STATUS
    {
        CLOSE_APP,
        RESUME
    };

    EngineInfo InitEngine(MemoryArena& a_arena, const wchar* a_app_name, const EngineOptions& a_engine_options, const GraphicOptions& a_graphic_options);
    bool DestroyEngine();
    ENGINE_STATUS UpdateEngine(const WindowHandle a_window_handle, const ConstSlice<InputEvent> a_input_events, const SwapchainWindow& a_main_window);
    SwapchainWindow& CreateSwapchainWindow(MemoryArena& a_arena, const OS_WINDOW_STYLE a_style, const uint2 a_extent, const int2 a_offset, const StringWView a_window_name);
    bool DestroySwapchainWindow(const SwapchainWindow& a_swapchain_window);

    const char* GetExePath();
    const StringView GetRootPath();

    bool WindowResized();
    bool WindowClosed();
}
