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

//IMMUTABLE SAMPLERS
_BBBIND(IMMUTABLE_SAMPLER_BASIC_BINDING, SPACE_IMMUTABLE_SAMPLER)SamplerState basic_3d_sampler;

//GLOBAL BINDINGS
_BBBIND(GLOBAL_VERTEX_BUFFER_BINDING, SPACE_GLOBAL)ByteAddressBuffer vertex_data;
_BBBIND(GLOBAL_CPU_VERTEX_BUFFER_BINDING, SPACE_GLOBAL)ByteAddressBuffer cpu_writable_vertex_data;
_BBBIND(GLOBAL_BUFFER_BINDING, SPACE_GLOBAL)ConstantBuffer<BB::GlobalRenderData> global_data;
_BBBIND(GLOBAL_BINDLESS_TEXTURES_BINDING, SPACE_GLOBAL)Texture2D textures_data[];
_BBBIND(GLOBAL_BINDLESS_TEXTURES_BINDING, SPACE_GLOBAL)Texture2DArray textures_array_data[];
_BBBIND(GLOBAL_BINDLESS_TEXTURES_BINDING, SPACE_GLOBAL)TextureCube textures_cube_data[];

//PER_SCENE BINDINGS
_BBBIND(PER_SCENE_SCENE_DATA_BINDING, SPACE_PER_SCENE)ByteAddressBuffer scene_data;
_BBBIND(PER_SCENE_TRANSFORM_DATA_BINDING, SPACE_PER_SCENE)ByteAddressBuffer transform_data;
_BBBIND(PER_SCENE_LIGHT_DATA_BINDING, SPACE_PER_SCENE)ByteAddressBuffer light_data;
_BBBIND(PER_SCENE_LIGHT_PROJECTION_VIEW_DATA_BINDING, SPACE_PER_SCENE)ByteAddressBuffer light_view_projection_data;

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

float CalculateShadow(const float4 a_frag_pos_light, const RDescriptorIndex a_shadow_map_texture, const uint a_shadow_map_base_layer)
{
    const float3 proj_coords = (a_frag_pos_light.xyz / a_frag_pos_light.w) * 0.5 + 0.5;
    const float closest_depth = textures_array_data[a_shadow_map_texture].Sample(basic_3d_sampler, float3(proj_coords.xy, float(a_shadow_map_base_layer))).r;
    const float current_depth = proj_coords.z;
    const float shadow = current_depth > closest_depth ? 1.0 : 0.0;
    
    return shadow;
}

// don't use this directly
float3 CalculateLight_impl(const float3 a_light_pos, const float3 a_color, const float specular_strength, const float3 a_normal, const float3 a_frag_pos, const float3 a_light_dir)
{
    const float3 light_dir = a_light_pos - a_frag_pos;
    // maybe normalize a_normal
    
    const float diff = max(dot(a_normal, light_dir), 0.0);
    const float3 diffuse = diff * a_color;
    
    return diffuse;
}

float3 CalculateLight(const BB::Light a_light, float3 a_normal, float3 a_frag_pos)
{
    if (a_light.light_type == POINT_LIGHT)
    {
        const float3 light_dir = normalize(a_light.pos.xyz - a_frag_pos);
        return CalculateLight_impl(a_light.pos.xyz, a_light.color.xyz, a_light.color.w, a_normal, a_frag_pos, light_dir);
    }
    else if (a_light.light_type == SPOT_LIGHT)
    {
        const float3 light_dir = normalize(a_light.pos.xyz - a_frag_pos);
        const float theta = dot(light_dir, normalize(-a_light.direction.xyz));
        if (theta > a_light.direction.w)
        {
            return CalculateLight_impl(a_light.pos.xyz, a_light.color.xyz, a_light.color.w, a_normal, a_frag_pos, light_dir);
        }
    }
    else if (a_light.light_type == DIRECTIONAL_LIGHT)
    {
        const float3 light_dir = normalize(-a_light.direction.xyz);
        return CalculateLight_impl(a_light.direction.xyz, a_light.color.xyz, a_light.color.w, a_normal, a_frag_pos, light_dir);
    }
    
    return float3(0.0, 0.0, 0.0);
}

#endif //COMMON_HLSL