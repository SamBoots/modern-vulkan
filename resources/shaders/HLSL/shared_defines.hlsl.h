#pragma once

#ifndef __HLSL_VERSION
#include "Common.h"
//most of the BB types share a similiar name to those of hlsl, good coincidence
#define uint uint32_t
namespace BB
{
	using RDescriptorIndex = FrameworkHandle32Bit<struct RDescriptorIndexTag>;
}

#define ALIGN_STRUCT(x) alignas(x)
#else // __HLSL_VERSION

#define ALIGN_STRUCT(x)
typedef uint RDescriptorIndex;

#endif // __HLSL_VERSION
