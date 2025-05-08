#pragma once
#include "Rendererfwd.hpp"
#include "Enginefwd.hpp"

namespace BB
{
    struct Line
    {
        float3 p0;
        Color p0_color;
        float3 p1;
        Color p1_color;
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
