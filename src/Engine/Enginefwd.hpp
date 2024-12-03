#pragma once
#include "Common.h"
#include <bitset>

namespace BB
{
	using MasterMaterialHandle = FrameworkHandle<struct MasterMaterialHandleTag>;
	using MaterialHandle = FrameworkHandle<struct MaterialHandleTag>;


	// ECS
	constexpr size_t MAX_ECS_COMPONENTS = 32;
	using ECSEntity = FrameworkHandle32Bit<struct ECSEntityHandle>;
	using ECSSignature = std::bitset<MAX_ECS_COMPONENTS>;
	using ECSSignatureIndex = FrameworkHandle32Bit<struct ECSSignatureIndexHandle>;
}
