#pragma once
#include "Common.h"
#include "Storage/BBString.h"
#include <bitset>

namespace BB
{
	using MasterMaterialHandle = FrameworkHandle<struct MasterMaterialHandleTag>;
	using MaterialHandle = FrameworkHandle<struct MaterialHandleTag>;
    using InputActionHandle = FrameworkHandle<struct InputActionHandleTag>;
    using InputChannelHandle = FrameworkHandle<struct InputChannelHandleTag>;
    using AssetHandle = FrameworkHandle<struct AssetHandleTag>;

	// ECS
	constexpr size_t MAX_ECS_COMPONENTS = 32;
	using ECSEntity = FrameworkHandle<struct ECSEntityHandle>;
	using ECSSignature = std::bitset<MAX_ECS_COMPONENTS>;
	using ECSSignatureIndex = FrameworkHandle32Bit<struct ECSSignatureIndexHandle>;


	constexpr ECSEntity INVALID_ECS_OBJ = ECSEntity(BB_INVALID_HANDLE_64);

    struct BoundingBox
    {
        float3 min;
        float3 max;
    };
}
