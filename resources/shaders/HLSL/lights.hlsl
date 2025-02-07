#ifndef LIGHTS_HLSL
#define LIGHTS_HLSL
#include "common.hlsl"

float CalculateShadowPCF_impl(const float4 a_world_pos_light, const float2 a_texture_xy, const RDescriptorIndex a_shadow_map_texture, const uint a_shadow_map_base_layer)
{
    const float4 proj_coords = a_world_pos_light / a_world_pos_light.w;
    const float2 texture_size = 1.0 / a_texture_xy;
    const float current_depth = proj_coords.z;
    if (proj_coords.z > 1.0)
        return 0.0;
    
    float shadow = 0;
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            const float3 sample_cords = float3(proj_coords.xy + float2(x, y) * texture_size, (float)a_shadow_map_base_layer);
            const float pcf_depth = textures_array_data[a_shadow_map_texture].Sample(shadow_map_sampler, sample_cords).r;
            shadow += current_depth > pcf_depth ? 1.0 : 0.0;
        }
    }
    
    return shadow / 9;
}

float CalculateShadow_impl(const float4 a_world_pos_light, const RDescriptorIndex a_shadow_map_texture, const uint a_shadow_map_base_layer)
{
    const float4 proj_coords = a_world_pos_light / a_world_pos_light.w;
    const float closest_depth = textures_array_data[a_shadow_map_texture].Sample(shadow_map_sampler, float3(proj_coords.xy, (float) a_shadow_map_base_layer)).r;
    const float current_depth = proj_coords.z;
    const float shadow = current_depth > closest_depth ? 1.0 : 0.0;
    if (proj_coords.z > 1.0)
        return 0.0;
    
    return shadow;
}

float CalculateShadowPCF(const float4 a_world_pos_light, const float2 a_texture_xy, const RDescriptorIndex a_shadow_map_texture, const uint a_shadow_map_base_layer)
{
    return CalculateShadowPCF_impl(a_world_pos_light, a_texture_xy, a_shadow_map_texture, a_shadow_map_base_layer);
}

float CalculateShadow(const float4 a_world_pos_light, const float2 a_texture_xy, const RDescriptorIndex a_shadow_map_texture, const uint a_shadow_map_base_layer)
{
    return CalculateShadow_impl(a_world_pos_light, a_shadow_map_texture, a_shadow_map_base_layer);
}

//float3 CalculatePointLight_impl(const BB::Light a_light, const float3 a_normal, const float3 a_world_pos, const float3 a_view_dir, const float a_shininess)
//{
//    const float3 light_dir = normalize(a_light.pos.xyz - a_world_pos);
//    float3 diffuse = CalculateDiffuse_impl(light_dir, a_light.color.xyz, a_normal, a_world_pos);
//    float3 specular = CalculateSpecular_impl(light_dir, a_light.color.xyz, a_normal, a_view_dir, a_world_pos, a_light.color.w, a_shininess);
//    
//    const float distance = length(a_light.pos.xyz - a_world_pos);
//    const float attenuation = 1.0 / (a_light.radius_constant + a_light.radius_linear * distance + a_light.radius_quadratic * (distance * distance));
//    
//    diffuse = diffuse * attenuation;
//    specular = specular * attenuation;
//    
//    return diffuse + specular;
//}

float DistributionGGX(const float3 a_N, const float3 a_H, const float a_roughness)
{
    float a      = a_roughness * a_roughness;
    float a2     = a*a;
    float N_dot_H  = max(dot(a_N, a_H), 0.0);
    float N_dot_H2 = N_dot_H * N_dot_H;
	
    float num   = a2;
    float denom = (N_dot_H2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(const float a_N_dot_V, const float a_roughness)
{
    float r = (a_roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = a_N_dot_V;
    float denom = a_N_dot_V * (1.0 - k) + k;
	
    return num / denom;
}

float GeometrySmith(const float3 a_N, const float3 a_V, const float3 a_L, const float a_roughness)
{
    float N_dot_V = max(dot(a_N, a_V), 0.0);
    float N_dot_L = max(dot(a_N, a_L), 0.0);
    float ggx2  = GeometrySchlickGGX(N_dot_V, a_roughness);
    float ggx1  = GeometrySchlickGGX(N_dot_L, a_roughness);
	
    return ggx1 * ggx2;
}

float3 FresnelSchlick(float a_cos_theta, float3 a_f0)
{
    return a_f0 + (1.0 - a_f0) * pow(clamp(1.0 - a_cos_theta, 0.0, 1.0), 5.0);
}

float3 PBRCalculateLight(const BB::Light a_light, const float3 a_L, const float3 a_V, const float3 a_N, const float3 a_albedo, const float3 a_f0, const float3 a_orm, const float3 a_world_pos)
{
    float3 light_dir;

    if (a_light.light_type == SPOT_LIGHT)
    {
        light_dir = normalize(a_light.direction.xyz);
        const float theta = dot(light_dir, normalize(-a_light.direction.xyz));
        if (theta <= a_light.direction.w)
        {
            const float N_dot_L = max(dot(a_N, light_dir), 0.0);
            return float3(scene_data.ambient_light.xyz * a_albedo);
        }
    }
    else if (a_light.light_type == DIRECTIONAL_LIGHT)
    {
        light_dir = normalize(-a_light.direction.xyz);
    }

    const float ao = a_orm.r;
    const float roughness = a_orm.g;
    const float metallic = a_orm.b;
    const float3 H = normalize(a_V + a_L);

    const float distance = length(a_light.pos.xyz - a_world_pos);
    const float attenuation = 1.0 / (a_light.radius_constant + a_light.radius_linear * distance + a_light.radius_quadratic * (distance * distance));
    const float3 radiance = a_light.color.xyz * attenuation; 

    const float NDF = DistributionGGX(a_N, H, roughness);
    const float G = GeometrySmith(a_N, a_V, a_L, roughness);
    const float3 fresnel = FresnelSchlick(max(dot(H, a_V), 0.0), a_f0);

    const float3 kD = (float3(1.0, 1.0, 1.0) - fresnel) * 1.0 - metallic;

    const float3 numerator = NDF * G * fresnel;
    const float denominator = 4.0 * max(dot(a_N, a_V), 0.0) * max(dot(a_N, a_L), 0.0) + 0.0001;
    const float3 specular = numerator / denominator;

    const float N_dot_L = max(dot(a_N, a_L), 0.0);
    return (kD * a_albedo / PI + specular) * radiance * N_dot_L;
}

float3 NonPBRCalculateLight(const BB::Light a_light, const float3 a_L, const float3 a_V, const float3 a_N, const float3 a_albedo, const float3 a_f0, const float3 a_orm, const float3 a_world_pos)
{
    float3 light_dir;

    if (a_light.light_type == SPOT_LIGHT)
    {
        // light_dir = normalize(a_light.pos.xyz - a_world_pos);
        const float theta = dot(light_dir, normalize(-a_light.direction.xyz));
        if (theta <= a_light.direction.w)
        {
            const float N_dot_L = max(dot(a_N, light_dir), 0.0);
            return float3(scene_data.ambient_light.xyz * a_albedo);
        }
    }
    else if (a_light.light_type == DIRECTIONAL_LIGHT)
    {
        light_dir = normalize(-a_light.direction.xyz);
    }

    const float diff = max(dot(a_N, light_dir), 0.0);
    const float3 diffuse = diff * a_light.color.xyz;
    
    const float3 reflect_dir = reflect(-light_dir, a_N);
    
    const float spec = pow(max(dot(a_V, reflect_dir), 0.0), 1.0);
    const float3 specular = a_light.color.w * spec * a_light.color.xyz;

    return diffuse + specular;
}

#endif //LIGHTS_HLSL
