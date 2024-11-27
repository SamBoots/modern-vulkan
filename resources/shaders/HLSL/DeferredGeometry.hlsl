#include "common.hlsl"

struct VSOutput
{
    float4 pos : SV_POSITION;
    _BBEXT(0)float3 frag_pos : POSITION0;
    _BBEXT(1)float4 color : COLOR0;
    _BBEXT(2)float2 uv : UV0;
    _BBEXT(3)float3x3 TBN : POSITION1;
};

_BBCONSTANT(BB::ShaderIndices) shader_indices;

VSOutput VertexMain(uint a_vertex_index : SV_VertexID)
{
    const uint vertex_offset = shader_indices.vertex_buffer_offset + sizeof(BB::Vertex) * a_vertex_index;
    BB::Vertex cur_vertex;
    cur_vertex.position = asfloat(vertex_data.Load3(vertex_offset));
    cur_vertex.normal = asfloat(vertex_data.Load3(vertex_offset + 12));
    cur_vertex.uv = asfloat(vertex_data.Load2(vertex_offset + 24));
    cur_vertex.color = asfloat(vertex_data.Load4(vertex_offset + 32));
    cur_vertex.tangent = asfloat(vertex_data.Load4(vertex_offset + 48));
   
    BB::ShaderTransform transform = transform_data.Load < BB::ShaderTransform > (
        sizeof(BB::ShaderTransform) * shader_indices.transform_index);
    
    const float3 T = normalize(mul(transform.transform, float4(cur_vertex.tangent.xyz, 1.0f)).xyz);
    const float3 N = normalize(mul(transform.transform, float4(cur_vertex.normal.xyz, 1.0f)).xyz);
    const float3 B = normalize(cross(N, T));
    const float3x3 TBN = transpose(float3x3(T, B, N));

    VSOutput output = (VSOutput) 0;
    output.frag_pos = float4(mul(transform.transform, float4(cur_vertex.position, 1.0f))).xyz;
    output.pos = mul(scene_data.proj, mul(scene_data.view, float4(output.frag_pos, 1.0)));
    output.uv = cur_vertex.uv;
    output.color = cur_vertex.color;
    output.TBN = TBN;
    
    return output;
}

struct PixelOutput
{
    float3 position : SV_Target0;
    float3 normal : SV_Target1;
    float3 albedo : SV_Target2;
};


PixelOutput FragmentMain(VSOutput a_input)
{
    const BB::MeshMetallic material = materials_metallic[shader_indices.material_index];

    const float3 normal = textures_data[material.normal_texture].Sample(basic_3d_sampler, a_input.uv).xyz * 2.0 - 1.0;
    const float4 texture_color = textures_data[material.albedo_texture].Sample(basic_3d_sampler, a_input.uv);

    PixelOutput output;
    output.position = output.frag_pos;
    output.normal = normalize(mul(a_input.TBN, normal));
    output.albedo = output.color.rgb * texture_color.rgb * material.base_color_factor.rgb;
    return output;
}
