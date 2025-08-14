#include "common.hlsl"

float4 VertexMain(uint a_vertex_index : SV_VertexID) : SV_POSITION
{
    BB::Scene3DInfo scene = GetSceneInfo();
    BB::ShaderIndicesShadowMapping shader_indices = PushConstantShadowMapping();

    const float3 cur_vertex_pos = GetAttributeFloat3(shader_indices.position_offset, a_vertex_index);
   
    BB::ShaderTransform transform = buffers[scene.matrix_index].Load<BB::ShaderTransform>(sizeof(BB::ShaderTransform) * shader_indices.transform_index);
    const float4x4 projview = buffers[scene.light_view_index].Load<float4x4>(sizeof(float4x4) * shader_indices.shadow_map_index);

    return mul(mul(projview, transform.transform), float4(cur_vertex_pos, 1.0));
}
