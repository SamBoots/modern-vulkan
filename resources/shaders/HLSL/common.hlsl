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
_BBBIND(IMMUTABLE_SAMPLER_SHADOW_MAP_BINDING, SPACE_IMMUTABLE_SAMPLER)SamplerState shadow_map_sampler;

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
    const float4 proj_coords = a_frag_pos_light / a_frag_pos_light.w;
    const float closest_depth = textures_array_data[a_shadow_map_texture].Sample(shadow_map_sampler, float3(proj_coords.xy, (float)a_shadow_map_base_layer)).r;
    const float current_depth = proj_coords.z;
    const float shadow = current_depth > closest_depth ? 1.0 : 0.0;
    return shadow;
}

float3 CalculateDiffuse_impl(const float3 a_light_dir, const float3 a_color, const float3 a_normal, const float3 a_frag_pos)
{
    const float diff = max(dot(a_normal, a_light_dir), 0.0);
    const float3 diffuse = diff * a_color;
   
    return diffuse;
}

float3 CalculateSpecular_impl(const float3 a_light_dir, const float3 a_color, const float3 a_normal, const float3 a_view_dir, const float3 a_frag_pos, const float a_specular_strength, const float a_shininess)
{
    const float3 view_dir = normalize(a_view_dir - a_frag_pos);
    const float3 reflect_dir = reflect(-a_light_dir, a_normal);
    
    const float spec = pow(max(dot(view_dir, reflect_dir), 0.0), a_shininess);
    const float3 specular = a_specular_strength * spec * a_color;

    return specular;
}

float3 CalculatePointLight_impl(const BB::Light a_light, const float3 a_normal, const float3 a_frag_pos, const float3 a_view_dir, const float a_shininess)
{
    const float3 light_dir = normalize(a_light.pos.xyz - a_frag_pos);
    float3 diffuse = CalculateDiffuse_impl(light_dir, a_light.color.xyz, a_normal, a_frag_pos);
    float3 specular = CalculateSpecular_impl(light_dir, a_light.color.xyz, a_normal, a_view_dir, a_frag_pos, a_light.color.w, a_shininess);
    
    const float distance = length(a_light.pos.xyz - a_frag_pos);
    const float attenuation = 1.0 / (a_light.radius_constant + a_light.radius_linear * distance + a_light.radius_quadratic * (distance * distance));
    
    diffuse = diffuse * attenuation;
    specular = specular * attenuation;
    
    return diffuse + specular;
}

float3 CalculateSpotLight_impl(const BB::Light a_light, const float3 a_normal, const float3 a_frag_pos, const float3 a_view_dir, const float a_shininess)
{
    const float3 light_dir = normalize(a_light.pos.xyz - a_frag_pos);
    const float theta = dot(light_dir, normalize(-a_light.direction.xyz));
    if (theta <= a_light.direction.w)
    {
        return float3(0.0, 0.0, 0.0);
    }
    
    const float3 diffuse = CalculateDiffuse_impl(light_dir, a_light.color.xyz, a_normal, a_frag_pos);
    const float3 specular = CalculateSpecular_impl(light_dir, a_light.color.xyz, a_normal, a_view_dir, a_frag_pos, a_light.color.w, a_shininess);
    
    return diffuse + specular;

}

float3 CalculateDirectionalLight_impl(const BB::Light a_light, const float3 a_normal, const float3 a_frag_pos, const float3 a_view_dir, const float a_shininess)
{
    const float3 light_dir = normalize(-a_light.direction.xyz);
    const float3 diffuse = CalculateDiffuse_impl(light_dir, a_light.color.xyz, a_normal, a_frag_pos);
    const float3 specular = CalculateSpecular_impl(light_dir, a_light.color.xyz, a_normal, a_view_dir, a_frag_pos, a_light.color.w, a_shininess);
    return diffuse + specular;
}

float3 CalculateLight(const BB::Light a_light, const float3 a_normal, const float3 a_frag_pos, const float3 a_view_dir, const float a_shininess)
{
    if (a_light.light_type == POINT_LIGHT)
    {
        return CalculatePointLight_impl(a_light, a_normal, a_frag_pos, a_view_dir, a_shininess);
    }
    else if (a_light.light_type == SPOT_LIGHT)
    {
        return CalculateSpotLight_impl(a_light, a_normal, a_frag_pos, a_view_dir, a_shininess);
    }
    else if (a_light.light_type == DIRECTIONAL_LIGHT)
    {
        return CalculateDirectionalLight_impl(a_light, a_normal, a_frag_pos, a_view_dir, a_shininess);
    }
    
    return float3(0.0, 0.0, 0.0);
}

#endif //COMMON_HLSL