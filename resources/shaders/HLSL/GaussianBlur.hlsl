#include "common.hlsl"

struct VSOutput
{
    float4 pos : SV_POSITION;
    _BBEXT(0)float2 uv : TEXCOORD0;
};

// thanks Sascha Willems https://www.saschawillems.de/blog/2016/08/13/vulkan-tutorial-on-rendering-a-fullscreen-quad-without-buffers/
        
VSOutput VertexMain(uint a_vertex_index : SV_VertexID)
{
    VSOutput output;
    output.uv = float2((a_vertex_index << 1) & 2, a_vertex_index & 2);
    output.pos = float4(output.uv * 2.0f + -1.0f, 0.0f, 1.0f);
    return output;
}

float4 FragmentMain(VSOutput a_input) : SV_Target
{
    const BB::ShaderGaussianBlur shader_indices = (BB::ShaderGaussianBlur)push_constant.userdata;

    float weight[5];
    weight[0] = 0.227027;
    weight[1] = 0.1945946;
    weight[2] = 0.1216216;
    weight[3] = 0.054054;
    weight[4] = 0.016216;

    const Texture2D texture = textures[shader_indices.src_texture];
    uint2 resolution;
    texture.GetDimensions(resolution.x, resolution.y);
    const float2 texture_offset = 1.0 / resolution * shader_indices.blur_scale;
    float3 result = texture.Sample(BASIC_3D_SAMPLER, a_input.uv).rgb * weight[0];
        
    if (shader_indices.horizontal_enable == 1)
    {
        for (int i = 1; i < 5; ++i)
        {
            result += texture.Sample(SHADOW_MAP_SAMPLER, a_input.uv + float2(texture_offset.x * i, 0.0)).rgb * weight[i] * shader_indices.blur_strength;
            result += texture.Sample(SHADOW_MAP_SAMPLER, a_input.uv - float2(texture_offset.x * i, 0.0)).rgb * weight[i] * shader_indices.blur_strength;
        }
    }
    else
    {
        for (int i = 1; i < 5; ++i)
        {
            result += texture.Sample(SHADOW_MAP_SAMPLER, a_input.uv + float2(0.0, texture_offset.y * i)).rgb * weight[i] * shader_indices.blur_strength;
            result += texture.Sample(SHADOW_MAP_SAMPLER, a_input.uv - float2(0.0, texture_offset.y * i)).rgb * weight[i] * shader_indices.blur_strength;
        }
    }
        
    return float4(result, 1.0);
}
