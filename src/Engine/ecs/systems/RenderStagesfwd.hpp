#pragma once
#include "Rendererfwd.hpp"
#include "Enginefwd.hpp"

namespace BB
{
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
