#include "common.hlsl"

struct VSOutput
{
    float4 pos : SV_POSITION;
    _BBEXT(0)float2 uv : TEXCOORD0;
};

// thanks Sascha Willems https://www.saschawillems.de/blog/2016/08/13/vulkan-tutorial-on-rendering-a-fullscreen-quad-without-buffers/
    
_BBCONSTANT(BB::ShaderGaussianBlur) shader_indices;
    
VSOutput VertexMain(uint a_vertex_index : SV_VertexID)
{
    VSOutput output;
    output.uv = float2((a_vertex_index << 1) & 2, a_vertex_index & 2);
    output.pos = float4(output.uv * 2.0f + -1.0f, 0.0f, 1.0f);
    return output;
}

float4 FragmentMain(VSOutput a_input) : SV_Target
{
    

    return float4(result, 1.0);
}
