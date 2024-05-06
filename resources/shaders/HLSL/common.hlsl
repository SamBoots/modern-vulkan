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

// don't use this
float3 CalculateLight_impl(const BB::Light a_light, const float3 a_normal, const float3 a_frag_pos, const float3 a_light_dir)
{   
    const float distance = length(a_light.pos - a_frag_pos);
    const float attenuation = 1.0f / (a_light.radius_constant, a_light.radius_linear * distance + a_light.radius_quadratic * (distance * distance));
    
    const float diff = max(dot(a_normal, a_light_dir), 0.0f);
    const float3 diffuse = mul(diff, a_light.color) * attenuation;
    
    const float3 view_pos = float3(0.f, 0.f, 0.f);
    const float3 view_dir = normalize(view_pos - a_frag_pos);
    const float3 reflect_dir = reflect(-a_light_dir, a_normal);
    
    const float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32);
    const float3 specular = a_light.specular_strength * spec * a_light.color;
    
    return (diffuse + specular);
}

float3 CalculateLight(const BB::Light a_light, float3 a_normal, float3 a_frag_pos)
{
    const float3 light_dir = normalize(a_light.pos - a_frag_pos);
    
    switch (a_light.light_type)
    {
        case POINT_LIGHT:
		{
            return CalculateLight_impl(a_light, a_normal, a_frag_pos, light_dir);
        }
        break;
        case SPOT_LIGHT:
		{
            const float theta = dot(light_dir, normalize(-a_light.spotlight_direction));
            if (theta > a_light.cutoff_radius)
            {
                return CalculateLight_impl(a_light, a_normal, a_frag_pos, light_dir);
            }
        }
        break;
    }
    return CalculateLight_impl(a_light, a_normal, a_frag_pos, light_dir);
}

//IMMUTABLE SAMPLERS
_BBBIND(IMMUTABLE_SAMPLER_BASIC_BINDING, SPACE_IMMUTABLE_SAMPLER)SamplerState basic_3d_sampler;

//GLOBAL BINDINGS
_BBBIND(GLOBAL_VERTEX_BUFFER_BINDING, SPACE_GLOBAL)ByteAddressBuffer vertex_data;
_BBBIND(GLOBAL_CPU_VERTEX_BUFFER_BINDING, SPACE_GLOBAL)ByteAddressBuffer cpu_writable_vertex_data;
_BBBIND(GLOBAL_BUFFER_BINDING, SPACE_GLOBAL)ConstantBuffer<BB::GlobalRenderData> global_data;
_BBBIND(GLOBAL_BINDLESS_TEXTURES_BINDING, SPACE_GLOBAL)Texture2D textures_data[];
_BBBIND(GLOBAL_BINDLESS_TEXTURES_BINDING, SPACE_GLOBAL)TextureCube textures_cube_data[];

//PER_SCENE BINDINGS
_BBBIND(PER_SCENE_SCENE_DATA_BINDING, SPACE_PER_SCENE)ByteAddressBuffer scene_data;
_BBBIND(PER_SCENE_TRANSFORM_DATA_BINDING, SPACE_PER_SCENE)ByteAddressBuffer transform_data;
_BBBIND(PER_SCENE_LIGHT_DATA_BINDING, SPACE_PER_SCENE)ByteAddressBuffer light_data;

#endif //COMMON_HLSL