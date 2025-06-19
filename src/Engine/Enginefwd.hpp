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

	// ECS
	constexpr size_t MAX_ECS_COMPONENTS = 32;
	using ECSEntity = FrameworkHandle<struct ECSEntityHandle>;
	using ECSSignature = std::bitset<MAX_ECS_COMPONENTS>;
	using ECSSignatureIndex = FrameworkHandle32Bit<struct ECSSignatureIndexHandle>;

	// String Forwards
	class PathString;

	constexpr ECSEntity INVALID_ECS_OBJ = ECSEntity(BB_INVALID_HANDLE_64);

    // used this to forward declare it
    class PathString : public StackString<MAX_PATH_SIZE>
    {
    public:
        using StackString<MAX_PATH_SIZE>::StackString;
    };

    struct BoundingBox
    {
        float3 min;
        float3 max;
    };
}
