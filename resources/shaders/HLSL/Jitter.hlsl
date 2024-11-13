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

float4 SnapVertex(const float4 pos, const float2 resolution)
{
    float4 new_pos = pos;
    new_pos.xyz = new_pos.xyz / new_pos.w;
    new_pos.xy = floor(resolution * new_pos.xy) / resolution;
    // 0-1
    float speed = 5.f;
    float magnitude = 0.05f;
    float frequency = 2;
    new_pos.y += sin(new_pos.x * frequency * 3.14f + global_data.total_time * speed) * magnitude;
    new_pos.x += sin(new_pos.y * frequency * 3.14f + global_data.total_time * speed) * magnitude;
    new_pos.xyz *= pos.w;
    return new_pos;
}

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

    float2 resolution = float2(scene_info.scene_resolution.xy / 16);
    float4 pos = mul(scene_info.proj, mul(scene_info.view, mul(transform.transform, float4(cur_vertex.position, 1.0))));
    float4 snap_pos = SnapVertex(pos, resolution);
    VSOutput output = (VSOutput)0;
    output.pos = snap_pos;
    output.frag_pos = float4(mul(transform.transform, float4(cur_vertex.position, 1.0f))).xyz;
    output.uv = cur_vertex.uv;
    output.color = cur_vertex.color;
    output.normal = normalize(mul(transform.inverse, float4(cur_vertex.normal.xyz, 0)).xyz);
    return output;
}