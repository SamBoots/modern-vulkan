#include "common.hlsl"

struct VSOutput
{
    float4 pos : SV_POSITION;
    _BBEXT(0)   float4 color : COLOR0;
    _BBEXT(1)   float2 uv : TEXCOORD0;
};

struct GuiInfo
{
    float2 uScale;
    float2 uTranslate;
    int textureIndex;
};

VSOutput VertexMain(uint VertexIndex : SV_VertexID)
{
    VSOutput output = (VSOutput) 0;
    output.pos = float4((input.inPosition * guiInfo.uScale) + guiInfo.uTranslate, 0, 1);
    output.color = input.color;
    output.uv = input.uv;
    return output;
}

float4 FragmentMain(VSoutput input) : SV_Target
{
    float4 color = input.color * text[guiInfo.textureIndex].Sample(samplerColor, input.uv);
    return color;
}