#pragma once
#include "shared_defines.hlsl.h"

#ifndef __HLSL_VERSION
//I don't like namespaces in my shaders
namespace BB
{
#endif//__HLSL_VERSION
    struct Vertex
    {
        float3 position; //12
        float3 normal; //24
        float2 uv; //32
        float3 color; //44 
    };

    struct SceneInfo
    {
        float4x4 view;
        float4x4 proj;

        float3 ambientLight;
        float ambientStrength;

        uint lightCount;
        uint3 padding;
    };

    struct ShaderTransform
    {
        float4x4 transform;
        float4x4 inverse;
    };

    struct ShaderIndices
    {
        uint vertex_buffer_offset;
        uint transform_index;
    };
#ifndef __HLSL_VERSION
}
#endif //__HLSL_VERSION