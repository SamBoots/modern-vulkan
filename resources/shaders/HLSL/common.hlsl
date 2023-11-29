#ifndef COMMON_HLSL
#define COMMON_HLSL
#include "shared_common.hlsl.h"

#ifdef _VULKAN
#define _BBEXT(num) [[vk::location(num)]]
#define _BBBIND(bind, set) [[vk::binding(bind, set)]]
#else
#define _BBEXT(num) [[vk::location(num)]]
#define _BBBIND(bind, set) [[vk::binding(bind, set)]]
#endif

#define unpack_uint_to_float4(a_uint) float4(float((a_uint & 0xff000000) >> 24), float((a_uint & 0x00ff0000) >> 16), float((a_uint & 0x0000ff00) >> 8), float((a_uint & 0x000000ff)))

_BBBIND(0, SPACE_PER_SCENE)ByteAddressBuffer scene_data;
_BBBIND(1, SPACE_PER_SCENE)ByteAddressBuffer transform_data;

_BBBIND(0, SPACE_IMMUTABLE_SAMPLER) SamplerState basic_3d_sampler;
_BBBIND(GLOBAL_VERTEX_BUFFER_BINDING, SPACE_GLOBAL)ByteAddressBuffer vertex_data;
_BBBIND(GLOBAL_CPU_VERTEX_BUFFER_BINDING, SPACE_GLOBAL)ByteAddressBuffer cpu_writable_vertex_data;
_BBBIND(GLOBAL_BINDLESS_TEXTURES_BINDING, SPACE_GLOBAL)Texture2D textures_data[];

#endif //COMMON_HLSL