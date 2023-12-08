#pragma once

#ifndef __HLSL_VERSION
#include "Common.h"
//most of the BB types share a similiar name to those of hlsl, good coincidence
#define uint uint32_t

#define HLSL_ENUM enum class
#else
#define HLSL_ENUM enum
#endif //IS HLSL
