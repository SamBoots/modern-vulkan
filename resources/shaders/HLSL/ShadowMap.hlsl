#include "common.hlsl"

float4 VertexMain(uint a_vertex_index : SV_VertexID, uint a_view_index : SV_ViewID) : SV_POSITION
{
    BB::Scene3DInfo scene = GetSceneInfo();
    BB::ShaderIndicesShadowMapping shader_indices = PushConstantShadowMapping();

    const float3 cur_vertex_pos = GetAttributeGeometry(shader_indices.geometry_offset, a_vertex_index);
   
    BB::ShaderTransform transform = buffers[scene.per_frame_index].Load<BB::ShaderTransform>(scene.matrix_offset + sizeof(BB::ShaderTransform) * shader_indices.transform_index);
    const float4x4 projview = buffers[scene.per_frame_index].Load<float4x4>(scene.light_view_offset + sizeof(float4x4) * a_view_index);

    return mul(mul(projview, transform.transform), float4(cur_vertex_pos, 1.0));
}
