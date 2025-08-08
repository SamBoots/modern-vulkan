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

// UNIQUE BINDINGS
_BBBIND(GPU_BINDING_GLOBAL, 0)ConstantBuffer<BB::GlobalRenderData> global_data;
_BBBIND(GPU_BINDING_SCENE, 0)ConstantBuffer<BB::Scene3DInfo> scene_data;

// ALL BINDINGS
_BBBIND(GPU_BINDING_IMAGES, 0)Texture2D textures[];
_BBBIND(GPU_BINDING_IMAGES, 0)Texture2DArray texture_arrays[];
_BBBIND(GPU_BINDING_SAMPLERS, 0)SamplerState samplers[];
_BBBIND(GPU_BINDING_BUFFERS, 0)ByteAddressBuffer buffers[];
_BBBIND(GPU_BINDING_BUFFERS, 0)RWByteAddressBuffer readwrite_buffers[];

// all uniforms
_BBBIND(GPU_BINDING_UNIFORMS, 0)ConstantBuffer<BB::MeshMetallic> uniform_metallic_info[];


float2 GetAttributeFloat2(const uint a_offset, const uint a_vertex_index, const uint)
{
     return asfloat(vertex_data.Load2(a_offset + sizeof(float2) * a_vertex_index));
}

float3 GetAttributeFloat3(const uint a_offset, const uint a_vertex_index)
{
     return asfloat(vertex_data.Load3(a_offset + sizeof(float3) * a_vertex_index));
}

float4 GetAttributeFloat4(const uint a_offset, const uint a_vertex_index)
{
    return asfloat(vertex_data.Load4(a_offset + sizeof(float4) * a_vertex_index));
}

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
    
float3 ReinhardToneMapping(const float3 a_hdr_color)
{
    return a_hdr_color / (a_hdr_color + float3(1.0, 1.0, 1.0));
}

float3 ExposureToneMapping(const float3 a_hdr_color, const float a_exposure)
{
    return float3(1.0, 1.0, 1.0) - exp(-a_hdr_color * a_exposure);
}

#endif //COMMON_HLSL
