#pragma once
#include "Rendererfwd.hpp"
#include "Enginefwd.hpp"

namespace BB
{
    struct LineColor
    {
        constexpr LineColor() : r(0), g(0), b(0), ignore_depth(false) {}
        constexpr explicit LineColor(const uint8_t a_r, const uint8_t a_g, const uint8_t a_b, const bool a_ignore_depth) : r(a_r), g(a_g), b(a_b), ignore_depth(a_ignore_depth) {}

        uint8_t r;
        uint8_t g;
        uint8_t b;
        uint8_t ignore_depth;
    };

    struct Line
    {
        float3 p0;
        LineColor p0_color;
        float3 p1;
        LineColor p1_color;
    };

    struct DrawList
    {
        struct DrawEntry
        {
            Mesh mesh;
            MasterMaterialHandle master_material;
            MaterialHandle material;
            uint32_t index_start;
            uint32_t index_count;
        };

        StaticArray<DrawEntry> draw_entries;
        StaticArray<ShaderTransform> transforms;
    };
}
