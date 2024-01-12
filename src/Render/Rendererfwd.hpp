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

	struct GPUBufferView;

	using RCommandPool = FrameworkHandle<struct RCommandPoolTag>;
	using RCommandList = FrameworkHandle<struct RCommandListTag>;

	using GPUBuffer = FrameworkHandle<struct RBufferTag>;
	//TEMP START
	using RImage = FrameworkHandle<struct RImageTag>;
	using RImageView = FrameworkHandle<struct RImageViewTag>;
	//TEMP END
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
		FRAGMENT_PIXEL	= 1 << 2
	};

	enum class IMAGE_FORMAT : uint32_t
	{
		RGBA16_UNORM,
		RGBA16_SFLOAT,

		RGBA8_SRGB,
		RGBA8_UNORM,

		A8_UNORM,

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

	enum class IMAGE_USAGE : uint32_t
	{
		DEPTH,
		TEXTURE,
		UPLOAD_SRC_DST, //maybe finally use bitflags.
		RENDER_TARGET,

		ENUM_SIZE
	};

	struct StartRenderingInfo
	{
		uint2 viewport_size;
		uint2 scissor_extent;
		int2 scissor_offset;

		bool load_color;
		bool store_color;
		IMAGE_LAYOUT layout;

		RImageView depth_view;

		float4 clear_color_rgba;
	};

	struct ScissorInfo
	{
		int2 offset;
		uint2 extent;
	};

	enum class BUFFER_TYPE : uint32_t
	{
		UPLOAD,
		STORAGE,
		UNIFORM,
		VERTEX,
		INDEX,

		ENUM_SIZE
	};

	struct GPUBufferCreateInfo
	{
		const char* name = nullptr;
		uint64_t size = 0;
		BUFFER_TYPE type{};
		bool host_writable = false;
	};

	struct GPUBufferView
	{
		GPUBuffer buffer;
		uint64_t size;
		uint64_t offset;
	};

	struct WriteableGPUBufferView
	{
		GPUBuffer buffer;
		uint64_t size;
		uint64_t offset;
		void* mapped;
	};
}
