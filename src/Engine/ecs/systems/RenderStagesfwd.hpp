#pragma once
#include "Rendererfwd.hpp"
#include "Enginefwd.hpp"

namespace BB
{
    struct Line
    {
        float3 p0;
        float3 p1;
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
