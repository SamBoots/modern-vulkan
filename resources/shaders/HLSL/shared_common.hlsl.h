#pragma once
#include "shared_defines.hlsl.h"

namespace BB
{
    //defined for HLSL and constexpr for the cpp
#define SPACE_IMMUTABLE_SAMPLER 0
#define SPACE_GLOBAL 1
#define SPACE_PER_SCENE 2
#define SPACE_PER_MATERIAL 3
#define SPACE_PER_MESH 4
#define SPACE_AMOUNT 5

#define STATIC_SAMPLER_MAX 1

#define IMMUTABLE_SAMPLER_BASIC_BINDING 0

#define GLOBAL_VERTEX_BUFFER_BINDING 0
#define GLOBAL_CPU_VERTEX_BUFFER_BINDING 1
#define GLOBAL_BINDLESS_TEXTURES_BINDING 2

#define PER_SCENE_SCENE_DATA_BINDING 0
#define PER_SCENE_TRANSFORM_DATA_BINDING 1
#define PER_SCENE_LIGHT_DATA_BINDING 2

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
        uint color;
    };

    struct Scene3DInfo
    {
        float4x4 view;
        float4x4 proj;

        float3 ambient_light;
        float ambient_strength;

        uint light_count;
        uint3 padding;
    };

    //pointlight
    struct PointLight
    {
        float3 color;           //12
        float3 pos;             //24

        float radius_linear;    //28
        float radius_quadratic; //32
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
}
