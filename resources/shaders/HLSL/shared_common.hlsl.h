#pragma once
#include "shared_defines.hlsl.h"

namespace BB
{
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
#define GLOBAL_BUFFER_BINDING 2
#define GLOBAL_BINDLESS_TEXTURES_BINDING 3 // must be last due to bindless

#define PER_SCENE_SCENE_DATA_BINDING 0
#define PER_SCENE_TRANSFORM_DATA_BINDING 1
#define PER_SCENE_LIGHT_DATA_BINDING 2

#define CUBEMAP_BACK    0
#define CUBEMAP_BOTTOM  1
#define CUBEMAP_FRONT   2
#define CUBEMAP_LEFT    3
#define CUBEMAP_RIGHT   4
#define CUBEMAP_TOP     5


    struct Vertex
    {
        float3 position;            // 12
        float3 normal;              // 24
        float2 uv;                  // 32
        float3 color;               // 44
    };

    struct VertexPos
    {
        float3 position;            // 12
    };

    struct Vertex2D
    {
        float2 position;    
        float2 uv;          
        uint color;         
    };

    struct GlobalRenderData
    {
        float2 mouse_pos;           // 8
        uint2 swapchain_resolution; // 16
        uint frame_index;           // 20
        uint frame_count;           // 24
        float delta_time;           // 28
        float total_time;           // 32
        uint cube_vertexpos_vertex_buffer_pos; // 36 used for cubemaps, the type is VertexPos
        uint padding;               // 40
    };

    struct Scene3DInfo
    {
        float4x4 view;              // 64
        float4x4 proj;              // 128

        float3 ambient_light;       // 136
        float ambient_strength;     // 144

        uint light_count;           // 148

        uint skybox_texture;        // 152

        uint2 scene_resolution;     // 156
    };

#ifndef __HLSL_VERSION // C++ version
    enum class LIGHT_TYPE : uint
    {
        POINT_LIGHT = 0,
        SPOT_LIGHT = 1
    };
#else   // HLSL version
#define POINT_LIGHT (0)
#define SPOT_LIGHT (1)
#endif //__HLSL_VERSION

    struct Light
    {
        float3 color;               // 12
        float3 pos;                 // 24

        float specular_strength;    // 28
        float radius_constant;      // 32
        float radius_linear;        // 36
        float radius_quadratic;     // 40

        float3 spotlight_direction; // 52
        float cutoff_radius;        // 56
        float pad;                  // 60
        uint light_type;            // 64
    };

    struct ShaderTransform
    {
        float4x4 transform;         // 64
        float4x4 inverse;           // 128
    };

    //could make the size the same for shaderindices and shaderindices2d so that the pushconstant pipelinelayout is the same.....
    struct ShaderIndices
    {
        uint vertex_buffer_offset;  // 4
        uint transform_index;       // 8
        uint albedo_texture;        // 12
        uint normal_texture;        // 16
        float2 padding;             // 24
    };

    struct ShaderIndices2D
    {
        uint vertex_buffer_offset;  // 4
        uint albedo_texture;        // 8
        float2 rect_scale;          // 16
        float2 translate;           // 24
    };
}
