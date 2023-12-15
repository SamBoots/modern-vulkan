#include "common.hlsl"

struct VSOutput
{
                float4 pos  : SV_POSITION;
    _BBEXT(0)   float4 color : COLOR0;
    _BBEXT(1)   float2 uv   : TEXCOORD0;

};

_BBCONSTANT(BB::ShaderIndices2D) shader_indices2D;

VSOutput VertexMain(uint a_vertex_index : SV_VertexID)
{
    const uint vertex_offset = shader_indices2D.vertex_buffer_offset + sizeof(BB::Vertex2D) * a_vertex_index;
    BB::Vertex2D cur_vertex;
    cur_vertex.position = asfloat(cpu_writable_vertex_data.Load2(vertex_offset));
    cur_vertex.uv = asfloat(cpu_writable_vertex_data.Load2(vertex_offset + 8));
    cur_vertex.color = cpu_writable_vertex_data.Load(vertex_offset + 16);
    
    VSOutput output = (VSOutput) 0;
    output.pos = float4((cur_vertex.position * shader_indices2D.rect_scale) + shader_indices2D.translate, 0, 1);
    output.color = UnpackR8B8G8A8_UNORMToFloat4(cur_vertex.color);
    output.uv = cur_vertex.uv;
    return output;
}

float4 FragmentMain(VSOutput a_input) : SV_Target
{
    float4 color = a_input.color * textures_data[shader_indices2D.albedo_texture].Sample(basic_3d_sampler, a_input.uv);
    return color;
}