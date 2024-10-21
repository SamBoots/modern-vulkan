#include "common.hlsl"

struct VSOutput
{
    float4 pos : SV_POSITION;
};

_BBCONSTANT(BB::ShaderIndicesShadowMapping) shader_indices;

VSOutput VertexMain(uint a_vertex_index : SV_VertexID)
{
    const uint vertex_offset = shader_indices.vertex_buffer_offset + sizeof(BB::Vertex) * a_vertex_index;
    float3 cur_vertex_pos = asfloat(vertex_data.Load3(vertex_offset));
   
    BB::ShaderTransform transform = transform_data.Load<BB::ShaderTransform>(
        sizeof(BB::ShaderTransform) * shader_indices.transform_index);
    
    BB::LightProjectionView projview = light_view_projection_data.Load<BB::LightProjectionView> (
        sizeof(BB::LightProjectionView) * shader_indices.light_projection_view_index);
    
    VSOutput output;
    output.pos = mul(projview.projection_view * transform.transform, float4(cur_vertex_pos, 1));
    return output;
}

void FragmentMain() : SV_TARGET
{
    //
}