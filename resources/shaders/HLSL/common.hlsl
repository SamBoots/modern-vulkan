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

float3 CalculatePointLight(const BB::PointLight a_light, float3 a_normal, float3 a_frag_pos)
{
    const float3 dir = normalize(a_light.pos - a_frag_pos);
    
    const float constant = 1.f;
    const float distance = length(a_light.pos - a_frag_pos);
    const float attenuation = 1.0f / (constant, a_light.radius_linear * distance + a_light.radius_quadratic * (distance * distance));
    
    float diff = max(dot(a_normal, dir), 0.0f);
    float3 diffuse = mul(diff, a_light.color);

    diffuse *= attenuation;
    
    float3 light_color = diffuse;
    return light_color;
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