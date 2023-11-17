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
	using MaterialHandle = FrameworkHandle<struct MaterialHandleTag>;

	enum class SHADER_STAGE : uint32_t
	{
		NONE,
		ALL,
		VERTEX,
		FRAGMENT_PIXEL,

		ENUM_SIZE
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
