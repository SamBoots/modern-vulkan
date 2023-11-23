#include "common.hlsl"

struct VSOutput
{
    float4 pos : SV_POSITION;
    _BBEXT(0)float3 frag_pos : POSITION0;
    _BBEXT(1)float3 color : COLOR0;
    _BBEXT(2)float2 uv : UV0;
    _BBEXT(3)float3 normal : NORMAL0;
};

VSOutput VertexMain(uint VertexIndex : SV_VertexID)
{
    SceneInfo scene_info = scene_data.Load<SceneInfo>(0);
    
    const uint vertex_offset = shader_indices.vertex_buffer_offset + sizeof(Vertex) * VertexIndex;
    Vertex cur_vertex;
    cur_vertex.position = asfloat(vertex_data.Load3(vertex_offset));
    cur_vertex.normal = asfloat(vertex_data.Load3(vertex_offset + 12));
    cur_vertex.uv = asfloat(vertex_data.Load2(vertex_offset + 24));
    cur_vertex.color = asfloat(vertex_data.Load3(vertex_offset + 32));
   
    ShaderTransform transform = transform_data.Load<ShaderTransform>(
        sizeof(ShaderTransform) * shader_indices.transform_index);
    
    VSOutput output = (VSOutput) 0;
    output.pos = mul(scene_info.proj, mul(scene_info.view, mul(transform.transform, float4(cur_vertex.position.xyz, 1.0))));
    output.frag_pos = float4(mul(transform.transform, float4(cur_vertex.position, 1.0f))).xyz;
    output.uv = cur_vertex.uv;
    output.color = cur_vertex.color;
    output.normal = mul(transform.inverse, float4(cur_vertex.normal.xyz, 0)).xyz;
    return output;
}

float4 FragmentMain(VSOutput input) : SV_Target
{
    float4 texture_color = textures_data[shader_indices.albedo_texture].Sample(samplerColor, input.uv);
    float4 color = texture_color * float4(input.color.xyz, 1.0f);
    
    return color;
}