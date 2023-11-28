#pragma once
#include "Common.h"
//shared shader include
#include "shared_common.hlsl.h"

namespace BB
{
	struct RenderIO
	{
		WindowHandle window_handle;
		uint32_t screen_width;
		uint32_t screen_height;

		uint32_t frame_index;
		uint32_t frame_count;
	};

	struct BufferView;

	using RCommandPool = FrameworkHandle<struct RCommandPoolTag>;
	using RCommandList = FrameworkHandle<struct RCommandListTag>;

	using RBuffer = FrameworkHandle<struct RBufferTag>;
	using RTexture = FrameworkHandle32Bit<struct RTextureTag>;
	using RDepthBuffer = FrameworkHandle<struct RDepthBufferTag>;

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

	enum class IMAGE_LAYOUT : uint32_t
	{
		UNDEFINED,
		GENERAL,
		TRANSFER_SRC,
		TRANSFER_DST,
		COLOR_ATTACHMENT_OPTIMAL,
		DEPTH_STENCIL_ATTACHMENT,
		SHADER_READ_ONLY,
		PRESENT,

		ENUM_SIZE
	};

	struct StartRenderingInfo
	{
		uint32_t viewport_width;
		uint32_t viewport_height;

		bool load_color;
		bool store_color;
		IMAGE_LAYOUT initial_layout;
		IMAGE_LAYOUT final_layout;

		RDepthBuffer depth_buffer;

		float4 clear_color_rgba;
	};

	struct EndRenderingInfo
	{
		IMAGE_LAYOUT initial_layout;
		IMAGE_LAYOUT final_layout;
	};

	struct ScissorInfo
	{
		int2 offset;
		uint2 extent;
	};
}
