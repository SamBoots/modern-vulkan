#include "common.hlsl"

struct VSOutput
{
    float4 pos : SV_POSITION;
    _BBEXT(0)float3 frag_pos : POSITION0;
    _BBEXT(1)float3 color : COLOR0;
    _BBEXT(2)float2 uv : UV0;
    _BBEXT(3)float3 normal : NORMAL0;
    _BBEXT(4)float4 frag_pos_light[8] : POSITION1;
};

_BBCONSTANT(BB::ShaderIndices) shader_indices;

static const float4x4 biasMat = float4x4(
	0.5, 0.0, 0.0, 0.5,
	0.0, 0.5, 0.0, 0.5,
	0.0, 0.0, 1.0, 0.0,
	0.0, 0.0, 0.0, 1.0);

VSOutput VertexMain(uint a_vertex_index : SV_VertexID)
{
    const BB::Scene3DInfo scene_info = scene_data;
    
    const uint vertex_offset = shader_indices.vertex_buffer_offset + sizeof(BB::Vertex) * a_vertex_index;
    BB::Vertex cur_vertex;
    cur_vertex.position = asfloat(vertex_data.Load3(vertex_offset));
    cur_vertex.normal = asfloat(vertex_data.Load3(vertex_offset + 12));
    cur_vertex.uv = asfloat(vertex_data.Load2(vertex_offset + 24));
    cur_vertex.color = asfloat(vertex_data.Load3(vertex_offset + 32));
   
    BB::ShaderTransform transform = transform_data.Load<BB::ShaderTransform>(
        sizeof(BB::ShaderTransform) * shader_indices.transform_index);
    
    VSOutput output = (VSOutput)0;
    output.frag_pos = float4(mul(transform.transform, float4(cur_vertex.position, 1.0f))).xyz;
    output.pos = mul(scene_info.proj, mul(scene_info.view, float4(output.frag_pos, 1.0)));
    output.uv = cur_vertex.uv;
    output.color = cur_vertex.color;
    output.normal = normalize(mul(transform.inverse, float4(cur_vertex.normal.xyz, 0)).xyz);
    
    for (uint i = 0; i < scene_info.light_count; i++)
    {
        const float4x4 projview = light_view_projection_data.Load<float4x4>(sizeof(float4x4) * i);
        output.frag_pos_light[i] = mul(biasMat, mul(projview, mul(transform.transform, float4(cur_vertex.position, 1.0))));
    }
    return output;
}

float4 FragmentMain(VSOutput a_input) : SV_Target
{
    const BB::Scene3DInfo scene_info = scene_data;
    
    const BB::MeshMetallic material = materials_metallic[shader_indices.material_index];
    
    const float4 texture_color = textures_data[material.albedo_texture].Sample(basic_3d_sampler, a_input.uv);
    const float4 color = float4(texture_color.xyz * a_input.color.xyz, 1.f);
    
    float3 result_color = 0;

    for (uint i = 0; i < scene_info.light_count; i++)
    {
        const BB::Light light = light_data.Load<BB::Light>(sizeof(BB::Light) * i);
        float3 diffuse = CalculateLight(light, a_input.normal, a_input.frag_pos, scene_info.view_pos.xyz, material.metallic_factor).xyz;
        float shadow = CalculateShadowPCF(a_input.frag_pos_light[i], scene_info.shadow_map_resolution, scene_info.shadow_map_array_descriptor, i);
        
        result_color += (1.0 - shadow) * (diffuse);
    }
    
    float4 result = float4(scene_info.ambient_light.xyz + result_color, 1.0) * color;
    
    return result;
}
