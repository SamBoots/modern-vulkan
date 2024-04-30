#pragma once
#include "Common.h"
//shared shader include
#include "shared_common.hlsl.h"
#include "Storage/Array.h"

namespace BB
{
	struct RenderIO
	{
		WindowHandle window_handle;
		uint32_t screen_width;
		uint32_t screen_height;

		uint32_t frame_index;
		uint32_t frame_count;

		//these will be set to false when an image is presented
		bool frame_started = false;
		bool frame_ended = false;
		bool resizing_request = false;
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

	using RenderScene3DHandle = FrameworkHandle<struct RenderScene3DHandleTag>;
	using LightHandle = FrameworkHandle<struct LightHandleTag>;
	using RenderTarget = FrameworkHandle<struct RenderTargetTag>;
	
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
		ENUM_SIZE = 4
	};

	enum class IMAGE_FORMAT : uint32_t
	{
		RGBA16_UNORM,
		RGBA16_SFLOAT,

		RGBA8_SRGB,
		RGBA8_UNORM,

		RGB8_SRGB,

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
		SWAPCHAIN_COPY_IMG, //maybe finally use bitflags.
		RENDER_TARGET,
		COPY_SRC_DST,

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
		READBACK,
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

	struct ImageCopyInfo
	{
		int32_t offset_x;
		int32_t offset_y;
		int32_t offset_z;

		uint32_t mip_level;
		uint16_t base_array_layer;
		uint16_t layer_count;
	};

	struct GPUDeviceInfo
	{
		char* name;

		struct MemoryHeapInfo
		{
			uint32_t heap_num;
			size_t heap_size;
			bool heap_device_local;
		};
		StaticArray<MemoryHeapInfo> memory_heaps;

		struct QueueFamily
		{
			uint32_t queue_family_index;
			uint32_t queue_count;
			bool support_compute;
			bool support_graphics;
			bool support_transfer;
		};
		StaticArray<QueueFamily> queue_families;
	};
}
