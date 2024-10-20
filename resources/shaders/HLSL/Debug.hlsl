#include "common.hlsl"

struct VSOutput
{
    float4 pos : SV_POSITION;
    _BBEXT(0)float3 frag_pos : POSITION0;
    _BBEXT(1)float3 color : COLOR0;
    _BBEXT(2)float2 uv : UV0;
    _BBEXT(3)float3 normal : NORMAL0;
    _BBEXT(4)float4 shadow_coord : POSITION1;
};

_BBCONSTANT(BB::ShaderIndices) shader_indices;

static const float4x4 shadow_bias_mat = float4x4(
	0.5, 0.0, 0.0, 0.5,
	0.0, 0.5, 0.0, 0.5,
	0.0, 0.0, 1.0, 0.0,
	0.0, 0.0, 0.0, 1.0);

VSOutput VertexMain(uint a_vertex_index : SV_VertexID)
{
    const BB::Scene3DInfo scene_info = scene_data.Load<BB::Scene3DInfo>(0);
    
    const uint vertex_offset = shader_indices.vertex_buffer_offset + sizeof(BB::Vertex) * a_vertex_index;
    BB::Vertex cur_vertex;
    cur_vertex.position = asfloat(vertex_data.Load3(vertex_offset));
    cur_vertex.normal = asfloat(vertex_data.Load3(vertex_offset + 12));
    cur_vertex.uv = asfloat(vertex_data.Load2(vertex_offset + 24));
    cur_vertex.color = asfloat(vertex_data.Load3(vertex_offset + 32));
   
    BB::ShaderTransform transform = transform_data.Load<BB::ShaderTransform>(
        sizeof(BB::ShaderTransform) * shader_indices.transform_index);
    
    const uint light_proj_view_temp = 0;
    BB::LightProjectionView projview = light_view_projection_data.Load<BB::LightProjectionView>(
        sizeof(BB::LightProjectionView) * light_proj_view_temp);
    
    VSOutput output = (VSOutput)0;
    output.pos = mul(scene_info.proj, mul(scene_info.view, mul(transform.transform, float4(cur_vertex.position, 1.0))));
    output.frag_pos = float4(mul(transform.transform, float4(cur_vertex.position, 1.0f))).xyz;
    output.uv = cur_vertex.uv;
    output.color = cur_vertex.color;
    output.normal = normalize(mul(transform.inverse, float4(cur_vertex.normal.xyz, 0)).xyz);
    output.shadow_coord = mul(shadow_bias_mat, mul(projview.projection_view, mul(transform.transform, float4(cur_vertex.position, 1))));
    return output;
}

float4 FragmentMain(VSOutput a_input) : SV_Target
{
    const BB::Scene3DInfo scene_info = scene_data.Load<BB::Scene3DInfo>(0);
    
    const float4 texture_color = textures_data[shader_indices.albedo_texture].Sample(basic_3d_sampler, a_input.uv);
    const float4 color = float4(texture_color.xyz * a_input.color.xyz, 1.f);
    
    float3 diffuse = 0;
    for (uint i = 0; i < scene_info.light_count; i++)
    {
        const BB::Light light = light_data.Load<BB::Light>(sizeof(BB::Light) * i);
        diffuse += CalculateLight(light, a_input.normal, a_input.frag_pos).xyz;
    }
    
    const float shadow = CalculateShadow(a_input.shadow_coord / a_input.shadow_coord.w, scene_info.shadow_map_array_descriptor, 0);
    const float4 diffuse_color = float4(diffuse, 1.0) * color;
    
    const float4 result = diffuse_color * shadow;
    
    return result;
}