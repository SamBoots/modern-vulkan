#include "common.hlsl"

// anti aliased lines https://atyuwen.github.io/posts/antialiased-line/

struct VSOutput
{
    float4 pos  : SV_POSITION;
    _BBEXT(0) float3 color: COLOR0;
};

_BBCONSTANT(BB::ShaderLine) shader_indices;

VSOutput VertexMain(uint a_vertex_index : SV_VertexID)
{
    const uint vertex_offset = shader_indices.vertex_start + a_vertex_index * sizeof(float4);
    const float3 pos = asfloat(cpu_writeable_vertex_data.Load3(vertex_offset));
    const uint color = cpu_writeable_vertex_data.Load(vertex_offset + sizeof(float3));

    VSOutput output = (VSOutput) 0;
    output.pos = mul(scene_data.proj, mul(scene_data.view, float4(pos, 1.0)));
    output.color = UnpackR8B8G8A8_UNORMToFloat4(color).xyz;
    return output;
}

struct GSOutput
{
	float4 pos : SV_Position;
     _BBEXT(0) float3 color : COLOR0;
     _BBEXT(1) noperspective float2 uv : UV0;
};

[maxvertexcount(4)]
void GeometryMain(line VSOutput a_in[2], inout TriangleStream<GSOutput> a_out) : SV_Target
{
    float4 p0 = a_in[0].pos;
	float4 p1 = a_in[1].pos;
    float3 p0_color = a_in[0].color;
    float3 p1_color = a_in[1].color;
	if (p0.w > p1.w)
	{
		const float4 temp = p0;
		p0 = p1;
		p1 = temp;
	}
    if (p0.w < scene_data.near_plane)
    {
        const float ratio = (scene_data.near_plane - p0.w) / (p1.w - p0.w);
        p0 = lerp (p0, p1, ratio);
    }

	const float2 a = p0.xy / p0.w;
	const float2 b = p1.xy / p1.w;
	const float2 c = normalize(float2(a.y - b.y, b.x - a.x)) / scene_data.scene_resolution.xy * 3;

	GSOutput g0;
	g0.pos = float4(p0.xy + c * p0.w, p0.zw);
    g0.color = p0_color;
    g0.uv = float2(shader_indices.line_width, 0.0);
	GSOutput g1;
	g1.pos = float4(p0.xy - c * p0.w, p0.zw);
    g1.color = p1_color;
    g1.uv = float2(-shader_indices.line_width, 0.0);
	GSOutput g2;
	g2.pos = float4(p1.xy + c * p1.w, p1.zw);
    g2.color = p0_color;
    g2.uv = float2(shader_indices.line_width, 0.0);
	GSOutput g3;
	g3.pos = float4(p1.xy - c * p1.w, p1.zw);
    g3.color = p1_color;
    g3.uv = float2(-shader_indices.line_width, 0.0);

	a_out.Append(g0);
	a_out.Append(g1);
	a_out.Append(g2);
	a_out.Append(g3);
    a_out.RestartStrip();
}

float4 FragmentMain(GSOutput a_input) : SV_Target
{
    const float a = exp2(-2.7 * a_input.uv.x * a_input.uv.x);
    return float4(a_input.color, a);
}
