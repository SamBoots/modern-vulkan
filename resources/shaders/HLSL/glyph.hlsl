#include "common.hlsl"

struct VSOutput
{
                float4 pos  : SV_POSITION;
    _BBEXT(0)   float2 uv   : TEXCOORD0;
};

VSOutput VertexMain(uint a_vertex_index : SV_VertexID, uint a_instance_index : SV_InstanceID)
{
    BB::Scene3DInfo scene = GetSceneInfo();
    BB::ShaderIndicesGlyph shader_indices = PushConstantGlyph();

    const BB::Glyph2D glyph = buffers[shader_indices.glyph_buffer_index].Load<BB::Glyph2D>(sizeof(BB::Glyph2D) * a_instance_index);
    
    float2 local = float2((a_vertex_index << 1) & 2, a_vertex_index & 2);
    float2 world = glyph.pos + local * glyph.extent * shader_indices.scale;
    float2 ndc = (world / scene.scene_resolution) * 2.0 - 1.0;

    float2 uv = lerp(glyph.uv0, glyph.uv1, local * 0.5);

    VSOutput output = (VSOutput) 0;
    output.pos = float4(ndc, 0.0, 1.0);
    output.uv = uv;
    return output;
}

float4 FragmentMain(VSOutput a_input) : SV_Target
{
    BB::ShaderIndicesGlyph shader_indices = PushConstantGlyph();

    float4 color = textures[shader_indices.font_texture].Sample(SHADOW_MAP_SAMPLER, a_input.uv).r;
    if (color.r < 0.01)
        discard;
    return color;
}