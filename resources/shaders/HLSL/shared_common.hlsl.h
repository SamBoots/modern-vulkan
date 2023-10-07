#pragma once
#include "shared_defines.hlsl.h"

namespace BB
{
    struct Vertex
    {
        float3 position; //12
        float3 normal; //24
        float2 uv; //32
        float3 color; //44 
    };

    struct ShaderIndices
    {
        uint vertex_buffer_offset;
        uint transform_index;
    };
}