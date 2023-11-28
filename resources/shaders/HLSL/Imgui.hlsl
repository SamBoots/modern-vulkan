#include "common.hlsl"

struct VSOutput
{
    float4 pos : SV_POSITION;
    _BBEXT(0)   float4 color : COLOR0;
    _BBEXT(1)   float2 uv : TEXCOORD0;
};

#ifdef _VULKAN
    [[vk::push_constant]] ShaderIndices2D shader_indices2D;
#else
    ConstantBuffer<ShaderIndices2D> shader_indices2D;
#endif

VSOutput VertexMain(uint VertexIndex : SV_VertexID)
{
    const uint vertex_offset = shader_indices2D.vertex_buffer_offset + sizeof(Vertex2D) * VertexIndex;
    Vertex2D cur_vertex;
    cur_vertex.position = asfloat(vertex_data.Load2(vertex_offset));
    cur_vertex.uv = asfloat(vertex_data.Load2(vertex_offset + 8));
    cur_vertex.color = asfloat(vertex_data.Load4(vertex_offset + 16));
    
    VSOutput output = (VSOutput) 0;
    output.pos = float4((cur_vertex.position * shader_indices2D.rect_scale) + shader_indices2D.translate, 0, 1);
    output.color = cur_vertex.color;
    output.uv = cur_vertex.uv;
    return output;
}

float4 FragmentMain(VSOutput input) : SV_Target
{
    float4 color = input.color * textures_data[shader_indices2D.albedo_texture].Sample(basic_3d_sampler, input.uv);
    return color;
}