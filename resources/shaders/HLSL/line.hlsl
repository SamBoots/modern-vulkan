#include "common.hlsl"

// anti aliased lines https://atyuwen.github.io/posts/antialiased-line/

struct VSOutput
{
    float4 pos  : SV_POSITION;
};

VSOutput VertexMain(uint a_vertex_index : SV_VertexID)
{
    const uint vertex_offset = a_vertex_index;
    const float3 pos = asfloat(cpu_writable_vertex_data.Load3(vertex_offset));

    VSOutput output = (VSOutput) 0;
    output.pos = mul(scene_data.proj, mul(scene_data.view, float4(pos, 1.0)));
    return output;
}

struct GSOutput
{
	float4 pos : SV_Position;
};

[maxvertexcount(2)]
void GeometryMain(line VSOutput a_in[2], inout TriangleStream<GSOutput> a_out) : SV_Target
{
    float4 p0 = a_in[0].pos;
	float4 p1 = a_in[1].pos;
	if (p0.w > p1.w)
	{
		const float4 temp = p0;
		p0 = p1;
		p1 = temp;
	}

	const float2 a = p0.xy / p0.w;
	const float2 b = p1.xy / p1.w;
	const float2 c = normalize(float2(a.y - b.y, b.x - a.x)) / scene_data.scene_resolution.xy * 3;

	GSOutput g0;
	g0.pos = float4(p0.xy + c * p0.w, p0.zw);
	GSOutput g1;
	g1.pos = float4(p0.xy - c * p0.w, p0.zw);
	GSOutput g2;
	g2.pos = float4(p1.xy + c * p1.w, p1.zw);
	GSOutput g3;
	g3.pos = float4(p1.xy - c * p1.w, p1.zw);

	a_out.Append(g0);
	a_out.Append(g1);
	a_out.Append(g2);
	a_out.Append(g3);
}

float4 FragmentMain(GSOutput a_input) : SV_Target
{
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}
