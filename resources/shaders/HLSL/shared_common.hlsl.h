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

#define STATIC_SAMPLER_MAX 2

#define IMMUTABLE_SAMPLER_BASIC_BINDING 0
#define IMMUTABLE_SAMPLER_SHADOW_MAP_BINDING 1

#define GLOBAL_VERTEX_BUFFER_BINDING 0
#define GLOBAL_CPU_VERTEX_BUFFER_BINDING 1
#define GLOBAL_BUFFER_BINDING 2
#define GLOBAL_BINDLESS_TEXTURES_BINDING 3 // must be last due to bindless

#define PER_SCENE_SCENE_DATA_BINDING 0
#define PER_SCENE_TRANSFORM_DATA_BINDING 1
#define PER_SCENE_LIGHT_DATA_BINDING 2
#define PER_SCENE_LIGHT_PROJECTION_VIEW_DATA_BINDING 3

#define PER_MATERIAL_BINDING 0

#define CUBEMAP_BACK    0
#define CUBEMAP_BOTTOM  1
#define CUBEMAP_FRONT   2
#define CUBEMAP_LEFT    3
#define CUBEMAP_RIGHT   4
#define CUBEMAP_TOP     5

    struct Vertex2D
    {
        float2 position;
        float2 uv;
        uint color;
    };

    struct ALIGN_STRUCT(16) GlobalRenderData
    {
        float2 mouse_pos;           // 8
        uint2 swapchain_resolution; // 16
        uint frame_index;           // 20
        uint frame_count;           // 24
        float delta_time;           // 28
        float total_time;           // 32
        float gamma;                // 36
        uint cube_vertexpos_vertex_buffer_pos; // 40 used for cubemaps, the type is VertexPos
        uint2 padding;               // 48
    };

    struct ALIGN_STRUCT(16) Scene3DInfo
    {
        float4x4 view;                   // 64
        float4x4 proj;                   // 128

        float4 ambient_light;            // 144

        uint2 scene_resolution;          // 152
        float2 shadow_map_resolution;    // 160

        float3 view_pos;                 // 172
        float exposure;                  // 176

        uint shadow_map_count;           // 180
        RDescriptorIndex shadow_map_array_descriptor; // 184
        uint light_count;                // 188
        RDescriptorIndex skybox_texture; // 192
        float near_plane;                // 196
        float3 pad;                      // 208
    };

    struct ALIGN_STRUCT(16) MeshMetallic
    {
        float4 base_color_factor;
        float roughness_factor;
        float metallic_factor;
        RDescriptorIndex albedo_texture;
        RDescriptorIndex normal_texture;
        RDescriptorIndex orm_texture;
        float3 pad;
    };

#ifndef __HLSL_VERSION // C++ version
    enum class LIGHT_TYPE : uint
    {
        POINT_LIGHT = 1,
        SPOT_LIGHT = 2,
        DIRECTIONAL_LIGHT = 3
    };
#else   // HLSL version
#define POINT_LIGHT (1)
#define SPOT_LIGHT (2)
#define DIRECTIONAL_LIGHT (3)
#endif //__HLSL_VERSION

    struct ALIGN_STRUCT(16) Light
    {
        float4 color;               // 16 color + w = specular strength
        float4 pos;                 // 32 w = padding
        float4 direction;           // 64 w = cutoff-radius

        float radius_constant;      // 40
        float radius_linear;        // 44
        float radius_quadratic;     // 48

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
        uint position_offset;            // 4
        uint normal_offset;              // 8
        uint uv_offset;                  // 12
        uint color_offset;               // 16
        uint tangent_offset;             // 20

        uint transform_index;            // 24
        RDescriptorIndex material_index; // 28
        float pad0;                      // 32
    };

    struct ShaderIndices2D
    {
        uint vertex_buffer_offset;       // 4
        RDescriptorIndex albedo_texture; // 8
        float2 rect_scale;               // 16
        float2 translate;                // 24
        uint2 pad0;                      // 32
    };

    struct ShaderIndicesShadowMapping
    {
        uint position_offset;             // 4
        uint transform_index;             // 8
        uint light_projection_view_index; // 12
        uint pad0;                        // 16
        uint4 pad1;                       // 32
    };

    struct ShaderGaussianBlur
    {
        RDescriptorIndex src_texture; // 4
        uint horizontal_enable;       // 8
        uint2 src_resolution;         // 16
        float blur_strength;          // 20
        float blur_scale;             // 24
        float2 pad0;                  // 32
    };

    struct ShaderLine
    {
        float line_width;
        uint vertex_start;
        uint2 pad0;
        uint4 pad1;
    };

#ifndef __HLSL_VERSION // C++ version
    static_assert(
        sizeof(ShaderIndices) == sizeof(ShaderIndices2D) &&
        sizeof(ShaderIndices) == sizeof(ShaderIndicesShadowMapping) &&
        sizeof(ShaderIndices) == sizeof(ShaderGaussianBlur) &&
        sizeof(ShaderIndices) == sizeof(ShaderLine));
#endif // __HLSL_VERSION
}
