#pragma once
#include "shared_defines.hlsl.h"

#ifndef __HLSL_VERSION
//I don't like namespaces in my shaders
namespace BB
{
#endif//__HLSL_VERSION

    //defined for HLSL and constexpr for the cpp
#ifdef __HLSL_VERSION
#define SPACE_IMMUTABLE_SAMPLER 0
#define SPACE_GLOBAL 1
#define SPACE_PER_SCENE 2
#define SPACE_PER_MATERIAL 3
#define SPACE_PER_MESH 4

#define GLOBAL_VERTEX_BUFFER_BINDING 0
#define GLOBAL_CPU_VERTEX_BUFFER_BINDING 1
#define GLOBAL_BINDLESS_TEXTURES_BINDING 2
#else
    constexpr uint32_t SPACE_IMMUTABLE_SAMPLER = 0;
    constexpr uint32_t SPACE_GLOBAL = 1;
    constexpr uint32_t SPACE_PER_SCENE = 2;
    constexpr uint32_t SPACE_PER_MATERIAL = 3;
    constexpr uint32_t SPACE_PER_MESH = 4;
    constexpr uint32_t SPACE_AMOUNT = 5;

    constexpr uint32_t STATIC_SAMPLER_MAX = 3;

    constexpr uint32_t GLOBAL_VERTEX_BUFFER_BINDING = 0;
    constexpr uint32_t GLOBAL_CPU_VERTEX_BUFFER_BINDING = 1;
    constexpr uint32_t GLOBAL_BINDLESS_TEXTURES_BINDING = 2;
#endif //__HLSL_VERSION

    struct Vertex
    {
        float3 position; //12
        float3 normal; //24
        float2 uv; //32
        float3 color; //44 
    };

    struct Vertex2D
    {
        float2 position;
        float2 uv;
        uint32_t color;
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

    //could make the size the same for shaderindices and shaderindices2d so that the pushconstant pipelinelayout is the same.....
    struct ShaderIndices
    {
        uint vertex_buffer_offset;  //4
        uint transform_index;       //8
        uint albedo_texture;        //12
        uint normal_texture;        //16
        float2 padding;             //24
    };
    
    struct ShaderIndices2D
    {
        uint vertex_buffer_offset;  //4
        uint albedo_texture;        //8
        float2 rect_scale;          //16
        float2 translate;           //24
    };

#ifndef __HLSL_VERSION
}
#endif //__HLSL_VERSION
