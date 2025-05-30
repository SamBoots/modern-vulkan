#pragma once
#include "Enginefwd.hpp"
#include "MemoryArena.hpp"

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

    EngineInfo InitEngine(MemoryArena& a_arena, const wchar* a_app_name, const EngineOptions& a_engine_options, const GraphicOptions& a_graphic_options);
    bool DestroyEngine();

    bool WindowResized();
    bool WindowClosed();
}
