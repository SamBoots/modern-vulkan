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


_BBBIND(0, 0)ByteAddressBuffer vertex_data;
_BBBIND(0, 1)ByteAddressBuffer scene_data;
_BBBIND(1, 1)ByteAddressBuffer transform_data;

#ifdef _VULKAN
    [[vk::push_constant]] ShaderIndices shader_indices;
#else
    ConstantBuffer<ShaderIndices> shader_indices;
#endif

#endif //COMMON_HLSL