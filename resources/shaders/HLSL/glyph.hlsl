#include "common.hlsl"

struct VSOutput
{
                float4 pos  : SV_POSITION;
    _BBEXT(0)   float4 color: COLOR0;
    _BBEXT(1)   float2 uv   : TEXCOORD0;
    _BBEXT(2)   uint text_id: BLENDINDICES0;
};

VSOutput VertexMain(uint a_vertex_index : SV_VertexID, uint a_instance_index : SV_InstanceID)
{
    BB::Scene3DInfo scene = GetSceneInfo();
    BB::ShaderIndices2DQuads shader_indices = PushConstant2DQuads();

    const BB::Quad2D quad = buffers[scene.per_frame_index].Load<BB::Quad2D>(shader_indices.per_frame_buffer_start + sizeof(BB::Quad2D) * a_instance_index);
    
    float2 local = float2((a_vertex_index << 1) & 2, a_vertex_index & 2) * 0.5;
    float2 world = quad.pos + local * quad.extent;
    float2 ndc = (world / scene.scene_resolution) - 1.0;
    
    float2 uv = lerp(quad.uv0, quad.uv1, local);

    VSOutput output = (VSOutput) 0;
    output.pos = float4(ndc, 0.0, 1.0);
    output.uv = uv;
    output.text_id = quad.text_id;
    output.color = UnpackR8B8G8A8_UNORMToFloat4(quad.color);
    return output;
}

float4 FragmentMain(VSOutput a_input) : SV_Target
{
    float4 color = a_input.color;
    color.a *= textures[a_input.text_id].Sample(UI_SAMPLER, a_input.uv).r;
    return color;
}