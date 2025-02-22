#include "common.hlsl"

struct VSOutput
{
    float4 pos : SV_POSITION;
    _BBEXT(0)float3 uvw : TEXCOORD0;
};

VSOutput VertexMain(uint a_vertex_index : SV_VertexID)
{
    const BB::Scene3DInfo scene_info = scene_data;
    
    const float3 position = GetAttributeFloat3(global_data.cube_vertexpos_vertex_buffer_pos, a_vertex_index);
    
    VSOutput output = (VSOutput)0;
    output.uvw = position;
    // Convert cubemap coordinates into Vulkan coordinate space
    //output.uvw.xy *= -1.0;
    // remove translation
    float4x4 mod_view = scene_info.view;
    mod_view[0][3] = 0.0;
    mod_view[1][3] = 0.0;
    mod_view[2][3] = 0.0;
    output.pos = mul(scene_info.proj, mul(mod_view, float4(position.xyz, 1.0)));
    return output;
}

float4 FragmentMain(VSOutput a_input) : SV_Target
{
    const BB::Scene3DInfo scene_info = scene_data;
    return textures_cube_data[scene_info.skybox_texture].Sample(basic_3d_sampler, a_input.uvw);
}
