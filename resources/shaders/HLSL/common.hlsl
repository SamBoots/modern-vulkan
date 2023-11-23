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

_BBBIND(0, SPACE_IMMUTABLE_SAMPLER) SamplerState samplerColor;
_BBBIND(0, SPACE_GLOBAL)ByteAddressBuffer vertex_data;
_BBBIND(1, SPACE_GLOBAL)Texture2D textures_data[];
_BBBIND(0, SPACE_PER_SCENE)ByteAddressBuffer scene_data;
_BBBIND(1, SPACE_PER_SCENE)ByteAddressBuffer transform_data;

#ifdef _VULKAN
    [[vk::push_constant]] ShaderIndices shader_indices;
#else
    ConstantBuffer<ShaderIndices> shader_indices;
#endif

#endif //COMMON_HLSL