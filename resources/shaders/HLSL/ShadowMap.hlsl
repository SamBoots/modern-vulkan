#include "common.hlsl"

float4 VertexMain(uint a_vertex_index : SV_VertexID) : SV_POSITION
{
    BB::ShaderIndicesShadowMapping pushc = (BB::ShaderIndicesShadowMapping)push_constant.userdata;

    const float3 cur_vertex_pos = GetAttributeFloat3(pushc.position_offset, a_vertex_index);
   
    BB::ShaderTransform transform = transform_data.Load<BB::ShaderTransform>(
        sizeof(BB::ShaderTransform) * pushc.transform_index);
    
    const float4x4 projview = buffers[GetSceneInfo().light_view_index].Load<float4x4>(sizeof(float4x4) * pushc.shadow_map_index);

    return mul(mul(projview, transform.transform), float4(cur_vertex_pos, 1.0));
}
