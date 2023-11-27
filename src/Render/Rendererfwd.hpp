#pragma once
#include "Common.h"
//shared shader include
#include "shared_common.hlsl.h"

namespace BB
{
	struct BufferView;

	using RCommandPool = FrameworkHandle<struct RCommandPoolTag>;
	using RCommandList = FrameworkHandle<struct RCommandListTag>;

	using RBuffer = FrameworkHandle<struct RBufferTag>;
	using RTexture = FrameworkHandle32Bit<struct RTextureTag>;

	using ShaderCode = FrameworkHandle<struct ShdaerCodeTag>;
	using MeshHandle = FrameworkHandle<struct MeshHandleTag>;
	using ShaderEffectHandle = FrameworkHandle<struct ShaderEffectHandleTag>;
	using MaterialHandle = FrameworkHandle<struct MaterialHandleTag>;

	constexpr uint32_t UNIQUE_SHADER_STAGE_COUNT = 2;
	using SHADER_STAGE_FLAGS = uint32_t;
	enum class SHADER_STAGE : SHADER_STAGE_FLAGS
	{
		NONE			= 0,
		ALL				= UINT32_MAX,
		VERTEX			= 1 << 1,
		FRAGMENT_PIXEL	= 1 << 2,

		ENUM_SIZE		= 4
	};

	enum class IMAGE_FORMAT : uint32_t
	{
		RGBA8_SRGB,
		RGBA8_UNORM,

		ENUM_SIZE
	};

	enum class IMAGE_TYPE : uint32_t
	{
		TYPE_1D,
		TYPE_2D,
		TYPE_3D,

		ENUM_SIZE
	};
}
