#pragma once
#include "Enginefwd.hpp"
#include "MemoryArena.hpp"
#include "Slice.h"
#include "HID.h"
#include "Storage/BBString.h"

namespace BB
{
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

    struct EngineInfo
    {
        WindowHandle window_handle;
        uint2 window_extent;
        uint32_t backbuffer_count;
    };

    enum class ENGINE_STATUS
    {
        RESIZE,
        CLOSE_APP,
        RESUME
    };

    EngineInfo InitEngine(MemoryArena& a_arena, const wchar* a_app_name, const EngineOptions& a_engine_options, const GraphicOptions& a_graphic_options);
    bool DestroyEngine();
    ENGINE_STATUS UpdateEngine(const WindowHandle a_window_handle, const ConstSlice<InputEvent> a_input_events);

    const char* GetExePath();
    const StringView GetRootPath();

    bool WindowResized();
    bool WindowClosed();
}
