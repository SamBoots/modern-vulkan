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

//IMMUTABLE SAMPLERS
_BBBIND(IMMUTABLE_SAMPLER_BASIC_BINDING, SPACE_IMMUTABLE_SAMPLER)SamplerState basic_3d_sampler;
_BBBIND(IMMUTABLE_SAMPLER_SHADOW_MAP_BINDING, SPACE_IMMUTABLE_SAMPLER)SamplerState shadow_map_sampler;

//GLOBAL BINDINGS
_BBBIND(GLOBAL_VERTEX_BUFFER_BINDING, SPACE_GLOBAL)ByteAddressBuffer vertex_data;
_BBBIND(GLOBAL_CPU_VERTEX_BUFFER_BINDING, SPACE_GLOBAL)ByteAddressBuffer cpu_writeable_vertex_data;
_BBBIND(GLOBAL_BUFFER_BINDING, SPACE_GLOBAL)ConstantBuffer<BB::GlobalRenderData> global_data;
_BBBIND(GLOBAL_BINDLESS_TEXTURES_BINDING, SPACE_GLOBAL)Texture2D textures_data[];
_BBBIND(GLOBAL_BINDLESS_TEXTURES_BINDING, SPACE_GLOBAL)Texture2DArray textures_array_data[];
_BBBIND(GLOBAL_BINDLESS_TEXTURES_BINDING, SPACE_GLOBAL)TextureCube textures_cube_data[];

//PER_SCENE BINDINGS
_BBBIND(PER_SCENE_SCENE_DATA_BINDING, SPACE_PER_SCENE)ConstantBuffer<BB::Scene3DInfo> scene_data;
_BBBIND(PER_SCENE_TRANSFORM_DATA_BINDING, SPACE_PER_SCENE)ByteAddressBuffer transform_data;
_BBBIND(PER_SCENE_LIGHT_DATA_BINDING, SPACE_PER_SCENE)ByteAddressBuffer light_data;
_BBBIND(PER_SCENE_LIGHT_PROJECTION_VIEW_DATA_BINDING, SPACE_PER_SCENE)ByteAddressBuffer light_view_projection_data;

//PER_MATERIAL BINDINGS

_BBBIND(PER_MATERIAL_BINDING, SPACE_PER_MATERIAL)ConstantBuffer<BB::MeshMetallic> materials_metallic[];

float2 GetAttributeFloat2(const uint a_offset, const uint a_vertex_index)
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
