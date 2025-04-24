#pragma once
#include "ecs/ECSBase.hpp"

namespace BB
{
	using PositionComponentPool = ECSComponentBase<float3, POSITION_ECS_SIGNATURE>;
	using RotationComponentPool = ECSComponentBase<float3x3, ROTATION_ECS_SIGNATURE>;
	using ScaleComponentPool = ECSComponentBase<float3, SCALE_ECS_SIGNATURE>;
	using LocalMatrixComponentPool = ECSComponentBase<float4x4, LOCAL_MATRIX_ECS_SIGNATURE>;
	using WorldMatrixComponentPool = ECSComponentBase<float4x4, WORLD_MATRIX_ECS_SIGNATURE>;
    using BoundingBoxComponentPool = ECSComponentBase<BoundingBox, BOUNDING_BOX_ECS_SIGNATURE>;
}
