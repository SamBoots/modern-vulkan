#ifndef COMMON_HLSL
#define COMMON_HLSL
#include "shared_common.hlsl.h"

#ifdef _VULKAN
#define _BBEXT(num) [[vk::location(num)]]
#define _BBBIND(bind, set) [[vk::binding(bind, set)]]
#define _BBCONSTANT(type) [[vk::push_constant]] type
#else
#define _BBEXT(num) [[vk::location(num)]]
#define _BBBIND(bind, set) [[vk::binding(bind, set)]]
#define _BBCONSTANT(type) [[vk::push_constant]] type
#endif

#define INVALID_TEXTURE 0
#define PI 3.14159265358979323846

_BBCONSTANT(BB::ShaderPushConstant) push_constant;

// UNIQUE BINDINGS
_BBBIND(GPU_BINDING_GLOBAL, 0)ConstantBuffer<BB::GlobalRenderData> global_data;

// ALL BINDINGS
_BBBIND(GPU_BINDING_IMAGES, 0)Texture2D textures[];
_BBBIND(GPU_BINDING_IMAGES, 0)Texture2DArray texture_arrays[];
_BBBIND(GPU_BINDING_IMAGES, 0)TextureCube texture_cubes[];

_BBBIND(GPU_BINDING_SAMPLERS, 0)SamplerState samplers[];
_BBBIND(GPU_BINDING_BUFFERS, 0)ByteAddressBuffer buffers[];
_BBBIND(GPU_BINDING_BUFFERS, 0)RWByteAddressBuffer readwrite_buffers[];

// all uniforms
_BBBIND(GPU_BINDING_UNIFORMS, 0)ConstantBuffer<BB::MeshMetallic> uniform_metallic_info[];
_BBBIND(GPU_BINDING_UNIFORMS, 0)ConstantBuffer<BB::Scene3DInfo> uniform_scene_info[];

// immutable samplers
_BBBIND(GPU_BINDING_IMMUTABLE_SAMPLERS, 0)SamplerState immutable_samplers[GPU_IMMUTABLE_SAMPLE_COUNT];
#define SHADOW_MAP_SAMPLER immutable_samplers[GPU_IMMUTABLE_SAMPLER_SHAWDOW_MAP]
#define BASIC_3D_SAMPLER immutable_samplers[GPU_IMMUTABLE_SAMPLER_TEMP]

float4 UnpackR8B8G8A8_UNORMToFloat4(uint a_packed)
{
    float4 unpacked;
    unpacked.x = float((a_packed >> 0) & 0xFF);
    unpacked.y = float((a_packed >> 8) & 0xFF);
    unpacked.z = float((a_packed >> 16) & 0xFF);
    unpacked.w = float((a_packed >> 24) & 0xFF);
    
    // taken from imgui, I assume most colors use this at least for UNORM
    float sc = 1.0f / 255.0f;
    return unpacked * sc;
}

float2 GetAttributeFloat2(const uint a_offset, const uint a_vertex_index)
{
     return asfloat(buffers[global_data.vertex_buffer].Load2(a_offset + sizeof(float2) * a_vertex_index));
}

float3 GetAttributeFloat3(const uint a_offset, const uint a_vertex_index)
{
     return asfloat(buffers[global_data.vertex_buffer].Load3(a_offset + sizeof(float3) * a_vertex_index));
}

float4 GetAttributeFloat4(const uint a_offset, const uint a_vertex_index)
{
    return asfloat(buffers[global_data.vertex_buffer].Load4(a_offset + sizeof(float4) * a_vertex_index));
}
    
float3 ReinhardToneMapping(const float3 a_hdr_color)
{
    return a_hdr_color / (a_hdr_color + float3(1.0, 1.0, 1.0));
}

float3 ExposureToneMapping(const float3 a_hdr_color, const float a_exposure)
{
    return float3(1.0, 1.0, 1.0) - exp(-a_hdr_color * a_exposure);
}

BB::Scene3DInfo GetSceneInfo()
{
    return uniform_scene_info[push_constant.scene_ub_index];
}

BB::PBRIndices PushConstantPBR()
{
    BB::PBRIndices indices;
    indices.vertex_offset = push_constant.userdata[0];
    indices.vertex_count = push_constant.userdata[1];
    indices.transform_index = push_constant.userdata[2];
    indices.material_index = push_constant.userdata[3];
    return indices;
}

BB::ShaderIndices2D PushConstant2D()
{
    BB::ShaderIndices2D indices;
    indices.vertex_offset = push_constant.userdata[0];
    indices.albedo_texture = push_constant.userdata[1];
    indices.rect_scale.x = asfloat(push_constant.userdata[2]);
    indices.rect_scale.y = asfloat(push_constant.userdata[3]);
    indices.translate.x = asfloat(push_constant.userdata[4]);
    indices.translate.y = asfloat(push_constant.userdata[5]);
    return indices;
}

BB::ShaderIndicesGlyph PushConstantGlyph()
{
    BB::ShaderIndicesGlyph indices;
    indices.glyph_buffer_offset = push_constant.userdata[0];
    indices.font_texture = push_constant.userdata[1];
    indices.scale.x = asfloat(push_constant.userdata[2]);
    indices.scale.y = asfloat(push_constant.userdata[3]);
    return indices;
}

BB::ShaderIndicesShadowMapping PushConstantShadowMapping()
{
    BB::ShaderIndicesShadowMapping indices;
    indices.position_offset = push_constant.userdata[0];
    indices.transform_index = push_constant.userdata[1];
    indices.shadow_map_index = push_constant.userdata[2];
    return indices;
}

BB::ShaderGaussianBlur PushConstantBlur()
{
    BB::ShaderGaussianBlur indices;
    indices.src_texture = push_constant.userdata[0];
    indices.horizontal_enable = push_constant.userdata[1];
    indices.src_resolution.x = push_constant.userdata[2];
    indices.src_resolution.y = push_constant.userdata[3];
    indices.blur_strength = asfloat(push_constant.userdata[4]);
    indices.blur_scale = asfloat(push_constant.userdata[5]);
    return indices;
}

BB::ShaderLine PushConstantLine()
{
    BB::ShaderLine indices;
    indices.line_width = asfloat(push_constant.userdata[0]);
    indices.vertex_start = push_constant.userdata[1];
    return indices;
}

#endif //COMMON_HLSL
