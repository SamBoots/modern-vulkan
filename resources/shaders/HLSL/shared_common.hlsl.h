#pragma once
#include "shared_defines.hlsl.h"

namespace BB
{
#define GPU_BINDING_GLOBAL 0
#define GPU_BINDING_IMAGES 1
#define GPU_BINDING_SAMPLERS 2
#define GPU_BINDING_BUFFERS 3
#define GPU_BINDING_UNIFORMS 4
#define GPU_BINDING_IMMUTABLE_SAMPLERS 5
#define GPU_BINDING_COUNT 6
// for GLOBAL and SCENE
#define GPU_EXTRA_UNIFORM_BUFFERS 2

#define GPU_IMMUTABLE_SAMPLER_SHAWDOW_MAP 0
#define GPU_IMMUTABLE_SAMPLER_TEMP 1
#define GPU_IMMUTABLE_SAMPLE_COUNT 2

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
        uint cubemap_geometry_offset; // 40 used for cubemaps, the type is VertexPos

        // buffers
        RDescriptorIndex geometry_buffer;           // 44
        RDescriptorIndex shading_buffer;            // 48
        RDescriptorIndex cpu_vertex_buffer;         // 52
        uint3 pad0;                                 // 64
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

        // buffers
        RDescriptorIndex matrix_index;            // 200
        RDescriptorIndex light_index;               // 204
        RDescriptorIndex light_view_index;          // 208
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

    struct Glyph2D
    {
        float2 pos;
        float2 uv;
    };

    struct ALIGN_STRUCT(16) PBRShadingAttribute
    {
        float3 normal;  // 12
        float3 tangent; // 24
        float2 uv;      // 32
        float4 color;   // 48
    };

    //could make the size the same for shaderindices and shaderindices2d so that the pushconstant pipelinelayout is the same.....
    struct PBRIndices
    {
        uint geometry_offset;           // 4
        uint shading_offset;            // 8
        uint transform_index;           // 12
        RDescriptorIndex material_index;// 16
    };

    struct ShaderIndices2D
    {
        uint vertex_offset;             // 4
        RDescriptorIndex albedo_texture;// 8
        float2 rect_scale;              // 16
        float2 translate;               // 24
    };

    struct ShaderIndicesGlyph
    {
        uint glyph_buffer_offset;       // 4
        RDescriptorIndex font_texture;  // 8
        float2 scale;                   // 16
    };

    struct ShaderIndicesShadowMapping
    {
        uint geometry_offset;           // 4
        uint transform_index;           // 8
        uint shadow_map_index;          // 12
    };

    struct ShaderGaussianBlur
    {
        RDescriptorIndex src_texture; // 4
        uint horizontal_enable;       // 8
        uint2 src_resolution;         // 16
        float blur_strength;          // 20
        float blur_scale;             // 24
    };

    struct ShaderLine
    {
        float line_width;   // 4
        uint vertex_start;  // 8
    };

    struct ShaderPushConstant
    {
        uint scene_ub_index;
        uint userdata[31];
    };
}
