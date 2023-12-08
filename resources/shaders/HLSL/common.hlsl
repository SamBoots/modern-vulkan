#ifndef COMMON_HLSL
#define COMMON_HLSL
#include "shared_common.hlsl.h"

#ifdef _VULKAN
#define _BBEXT(num) [[vk::location(num)]]
#define _BBBIND(bind, set) [[vk::binding(bind, set)]]
#else
#define _BBEXT(num) [[vk::location(num)]]
#define _BBBIND(bind, set) [[vk::binding(bind, set)]]
#endif

#define unpack_uint_to_float4(a_uint) float4(float((a_uint & 0xff000000) >> 24), float((a_uint & 0x00ff0000) >> 16), float((a_uint & 0x0000ff00) >> 8), float((a_uint & 0x000000ff) >> 0))
#define unpack_uint_to_uint4(a_uint) uint4(uint((a_uint & 0xff000000) >> 24), uint((a_uint & 0x00ff0000) >> 16), uint((a_uint & 0x0000ff00) >> 8), uint((a_uint & 0x000000ff) >> 0))

float4 PackedUintToFloat4Color(const uint a_uint)
{
    float4 unpacked = unpack_uint_to_float4(a_uint);
    unpacked = unpacked / 255;
    unpacked[3] = 1;
    return unpacked;
}

float4 CalculatePointLight(const PointLight a_light, float3 a_normal, float3 a_frag_pos)
{
    float3 dir = normalize(a_light.pos - a_frag_pos);

    float diff = max(dot(a_normal, dir), 0.0f);
    return float4(mul(diff, a_light.color));
}

//IMMUTABLE SAMPLERS
_BBBIND(IMMUTABLE_SAMPLER_BASIC_BINDING, SPACE_IMMUTABLE_SAMPLER)SamplerState basic_3d_sampler;

//GLOBAL BINDINGS
_BBBIND(GLOBAL_VERTEX_BUFFER_BINDING, SPACE_GLOBAL)ByteAddressBuffer vertex_data;
_BBBIND(GLOBAL_CPU_VERTEX_BUFFER_BINDING, SPACE_GLOBAL)ByteAddressBuffer cpu_writable_vertex_data;
_BBBIND(GLOBAL_BINDLESS_TEXTURES_BINDING, SPACE_GLOBAL)Texture2D textures_data[];

//PER_SCENE BINDINGS
_BBBIND(PER_SCENE_SCENE_DATA_BINDING, SPACE_PER_SCENE)ByteAddressBuffer scene_data;
_BBBIND(PER_SCENE_TRANSFORM_DATA_BINDING, SPACE_PER_SCENE)ByteAddressBuffer transform_data;
_BBBIND(PER_SCENE_LIGHT_DATA_BINDING, SPACE_PER_SCENE)ByteAddressBuffer light_data;

#endif //COMMON_HLSL