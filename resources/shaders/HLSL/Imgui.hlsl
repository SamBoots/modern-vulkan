#include "common.hlsl"

struct VSOutput
{
                float4 pos  : SV_POSITION;
    _BBEXT(0)   float4 color : COLOR0;
    _BBEXT(1)   float2 uv   : TEXCOORD0;
};

VSOutput VertexMain(uint a_vertex_index : SV_VertexID)
{
    BB::ShaderIndices2D shader_indices = PushConstant2D();

    const uint vertex_offset = shader_indices.vertex_offset + sizeof(BB::Vertex2D) * a_vertex_index;
    BB::Vertex2D cur_vertex;

    cur_vertex.position = asfloat(buffers[global_data.cpu_vertex_buffer].Load2(vertex_offset));
    cur_vertex.uv = asfloat(buffers[global_data.cpu_vertex_buffer].Load2(vertex_offset + 8));
    cur_vertex.color = buffers[global_data.cpu_vertex_buffer].Load(vertex_offset + 16);
    
    VSOutput output = (VSOutput) 0;
    output.pos = float4((cur_vertex.position * shader_indices.rect_scale) + shader_indices.translate, 0, 1);
    output.color = UnpackR8B8G8A8_UNORMToFloat4(cur_vertex.color);
    output.uv = cur_vertex.uv;
    return output;
}

float4 FragmentMain(VSOutput a_input) : SV_Target
{
    BB::ShaderIndices2D shader_indices = PushConstant2D();

    float4 color = a_input.color * textures[shader_indices.albedo_texture].Sample(UI_SAMPLER, a_input.uv);
    return color;
}