#include "common.hlsl"
#include "lights.hlsl"

struct VSOutput
{
    float4 pos : SV_POSITION;
    _BBEXT(0)float3 world_pos : POSITION0;
    _BBEXT(1)float4 color : COLOR0;
    _BBEXT(2)float2 uv : UV0;
    _BBEXT(3)float3x3 TBN : POSITION1;
    _BBEXT(6)float4 world_pos_light[8] : POSITION2;
};

_BBCONSTANT(BB::ShaderIndices) shader_indices;

static const float4x4 biasMat = float4x4(
	0.5, 0.0, 0.0, 0.5,
	0.0, 0.5, 0.0, 0.5,
	0.0, 0.0, 1.0, 0.0,
	0.0, 0.0, 0.0, 1.0);

VSOutput VertexMain(uint a_vertex_index : SV_VertexID)
{
    const float3 position = GetAttributeFloat3(shader_indices.position_offset, a_vertex_index);
    const float3 normal = GetAttributeFloat3(shader_indices.normal_offset, a_vertex_index);
    const float2 uv = GetAttributeFloat2(shader_indices.uv_offset, a_vertex_index);
    const float4 color = GetAttributeFloat4(shader_indices.color_offset, a_vertex_index);
    const float3 tangent = GetAttributeFloat3(shader_indices.tangent_offset, a_vertex_index);
   
    BB::ShaderTransform transform = transform_data.Load<BB::ShaderTransform>(sizeof(BB::ShaderTransform) * shader_indices.transform_index);
    
    float3x3 normalMatrix = (float3x3)transpose(transform.inverse);
    float3 T = normalize(mul(normalMatrix, tangent));
    const float3 N = normalize(mul(normalMatrix, normal));
    T = normalize(T - dot(T, N) * N);
    const float3 B = cross(N, T);
    const float3x3 TBN = transpose(float3x3(T, B, N));

    VSOutput output = (VSOutput) 0;
    output.world_pos = float4(mul(transform.transform, float4(position, 1.0f))).xyz;
    output.pos = mul(scene_data.proj, mul(scene_data.view, float4(output.world_pos, 1.0)));
    output.uv = uv;
    output.color = color;
    output.TBN = TBN;
    
    for (uint i = 0; i < scene_data.light_count; i++)
    {
        const float4x4 projview = light_view_projection_data.Load<float4x4>(sizeof(float4x4) * i);
        output.world_pos_light[i] = mul(biasMat, mul(projview, mul(transform.transform, float4(position, 1.0))));
    }
    return output;
}

struct PixelOutput
{
    float4 color : SV_Target0;
    float4 bloom : SV_Target1;
};

PixelOutput FragmentMain(VSOutput a_input)
{
    const BB::MeshMetallic material = materials_metallic[shader_indices.material_index];

    const float3 normal_map = textures_data[material.normal_texture].Sample(basic_3d_sampler, a_input.uv).xyz * 2.0 - 1.0;
    const float3 N = normalize(mul(a_input.TBN, normal_map));
    const float3 V = normalize(scene_data.view_pos - a_input.world_pos);

    float3 orm_data = float3(0.0, 0.0, 0.0);
    if (material.orm_texture != INVALID_TEXTURE)
    {
        orm_data = textures_data[material.orm_texture].Sample(basic_3d_sampler, a_input.uv).xyz;
        orm_data.g = orm_data.g * material.roughness_factor;
        orm_data.b = orm_data.b * material.metallic_factor;
    }
    else
    {
		orm_data.r = 1.0f;
        orm_data.g = clamp(material.roughness_factor, 0.04, 1.0);
        orm_data.b = clamp(material.metallic_factor, 0.0, 1.0);
    }

    const float3 albedo = textures_data[material.albedo_texture].Sample(basic_3d_sampler, a_input.uv).xyz;// * a_input.color.xyz * material.base_color_factor.xyz;
    const float3 f0 = lerp(0.04, albedo.xyz, orm_data.b);

    float3 lo = float3(0.0, 0.0, 0.0);
    for (uint i = 0; i < scene_data.light_count; i++)
    {
        const BB::Light light = light_data.Load<BB::Light>(sizeof(BB::Light) * i);
        const float3 L = normalize(light.pos.xyz - a_input.world_pos);

        const float3 light_color = PBRCalculateLight(light, L, V, N, albedo, f0, orm_data, a_input.world_pos);
        const float shadow = CalculateShadowPCF(a_input.world_pos_light[i], scene_data.shadow_map_resolution, scene_data.shadow_map_array_descriptor, i);
        
        lo += (1.0 - shadow) * (light_color);
    }

    const float3 ambient = scene_data.ambient_light.xyz * albedo * orm_data.r;
    float3 color = ambient + lo;

    PixelOutput output;

    // bloom
    const float brightness = dot(color.rgb, float3(0.2126, 0.7152, 0.0722));
    if (brightness > 1.0)
        output.bloom = float4(color.rgb, 1.0);
    else
        output.bloom = float4(0.0, 0.0, 0.0, 1.0);

    color = color / (color + float3(1.0, 1.0, 1.0));
    color = pow(color, 1.0f/2.2f);
    color = ExposureToneMapping(color, scene_data.exposure);
    output.color = float4(color, 1.0);
    
    return output;
}
