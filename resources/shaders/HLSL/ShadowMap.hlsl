#include "common.hlsl"

_BBCONSTANT(BB::ShaderIndicesShadowMapping) shader_indices;

float4 VertexMain(uint a_vertex_index : SV_VertexID) : SV_POSITION
{
    const uint vertex_offset = shader_indices.vertex_buffer_offset + sizeof(BB::Vertex) * a_vertex_index;
    const float3 cur_vertex_pos = asfloat(vertex_data.Load3(vertex_offset));
   
    BB::ShaderTransform transform = transform_data.Load<BB::ShaderTransform>(
        sizeof(BB::ShaderTransform) * shader_indices.transform_index);
    
    const float4x4 projview = light_view_projection_data.Load<float4x4>(sizeof(float4x4) * shader_indices.light_projection_view_index);

    return mul(mul(projview, transform.transform), float4(cur_vertex_pos, 1.0));
}
