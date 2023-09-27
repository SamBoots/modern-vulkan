#ifdef _VULKAN
#define _BBEXT(num) [[vk::location(num)]]
#define _BBBIND(bind, set) [[vk::binding(bind, set)]]
#else
#define _BBEXT(num) [[vk::location(num)]]
#define _BBBIND(bind, set) [[vk::binding(bind, set)]]
#endif

struct Vertex
{
    float3 position; //12
    float3 normal; //24
    float2 uv; //32
    float3 color; //44 
};

struct VSOutput
{
    //not sure if needed, check directx12 later.
    float4 pos : SV_POSITION;
    _BBEXT(0)  float3 color : COLOR0;
    _BBEXT(1)  float2 uv : UV0;
    _BBEXT(2)  float3 normal : NORMAL0;
};

_BBBIND(0, 0) ByteAddressBuffer vertex_data;

VSOutput VertexMain(uint VertexIndex : SV_VertexID)
{
    const uint vertex_offset = sizeof(Vertex) * VertexIndex;
    Vertex cur_vertex;
    cur_vertex.position = asfloat(vertex_data.Load3(vertex_offset));
    cur_vertex.normal = asfloat(vertex_data.Load3(vertex_offset + 12));
    cur_vertex.uv = asfloat(vertex_data.Load2(vertex_offset + 24));
    cur_vertex.color = asfloat(vertex_data.Load3(vertex_offset + 32));
    
    VSOutput output = (VSOutput) 0;
    output.pos = float4(cur_vertex.position.xyz, 1.0);
    output.uv = cur_vertex.uv;
    output.color = cur_vertex.color;
    output.normal = float4(cur_vertex.normal.xyz, 0).xyz;
    return output;
}

float4 FragmentMain(VSOutput input) : SV_Target
{
    return float4(input.color.xyz, 1.f);
}