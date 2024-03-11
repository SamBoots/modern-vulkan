#include "common.hlsl"

struct VSOutput
{
    float4 pos : SV_POSITION;
    _BBEXT(0)float3 frag_pos : POSITION0;
    _BBEXT(1)float3 color : COLOR0;
    _BBEXT(2)float2 uv : UV0;
    _BBEXT(3)float3 normal : NORMAL0;
};

_BBCONSTANT(BB::ShaderIndices) shader_indices;

float4 SnapVertex(const float4 pos, const float2 resolution, float a_time)
{
    float4 new_pos = pos;
    new_pos.xyz = new_pos.xyz / new_pos.w;
    new_pos.xy = floor(resolution * new_pos.xy) / resolution;
    // 0-1
    float speed = 10.f;
    float magnitude = 0.005f;
    float frequency = 40;
    new_pos.y += sin(new_pos.x * frequency * 3.14f + a_time * speed) * magnitude;
    new_pos.x += sin(new_pos.y * frequency * 3.14f + a_time * speed) * magnitude;
    new_pos.xyz *= pos.w;
    return new_pos;
}

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

    float2 resolution = float2(120.f, 60.f);
    float4 pos = mul(scene_info.proj, mul(scene_info.view, mul(transform.transform, float4(cur_vertex.position, 1.0))));
    float4 snap_pos = SnapVertex(pos, resolution, scene_info.time);
    VSOutput output = (VSOutput)0;
    output.pos = snap_pos;
    output.frag_pos = float4(mul(transform.transform, float4(cur_vertex.position, 1.0f))).xyz;
    output.uv = cur_vertex.uv;
    output.color = cur_vertex.color;
    output.normal = normalize(mul(transform.inverse, float4(cur_vertex.normal.xyz, 0)).xyz);
    return output;
}

float4 FragmentMain(VSOutput a_input) : SV_Target
{
    const BB::Scene3DInfo scene_info = scene_data.Load<BB::Scene3DInfo>(0);
    
    float4 texture_color = textures_data[shader_indices.albedo_texture].Sample(basic_3d_sampler, a_input.uv);
    float4 color = float4(texture_color.xyz * a_input.color.xyz, 1.f);
    
    float3 diffuse = 0;
    for (uint i = 0; i < scene_info.light_count; i++)
    {
        const BB::PointLight point_light = light_data.Load<BB::PointLight>(sizeof(BB::PointLight) * i);
        diffuse += CalculatePointLight(point_light, a_input.normal, a_input.frag_pos).xyz;
    }
    float4 result = float4(diffuse, 1.f) * color;
    
    return result;
}