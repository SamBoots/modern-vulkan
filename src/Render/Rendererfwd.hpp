#pragma once
#include "Common.h"
//shared shader include
#include "shared_common.hlsl.h"
#include "Utils/Slice.h"

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

	using RDescriptorLayout = FrameworkHandle<struct RDescriptorLayoutTag>;
	using GPUFenceValue = FrameworkHandle<struct GPUFenceValueTag>;

	using GPUBuffer = FrameworkHandle<struct RBufferTag>;
	//TEMP START
	using RImage = FrameworkHandle<struct RImageTag>;
	using RImageView = FrameworkHandle<struct RImageViewTag>;
	//TEMP END
	using RTexture = FrameworkHandle32Bit<struct RTextureTag>;

	using RFence = FrameworkHandle<struct RFenceTag>;

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
		ENUM_SIZE		= 4
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

	enum class IMAGE_VIEW_TYPE : uint32_t
	{
		TYPE_1D,
		TYPE_2D,
		TYPE_3D,
		CUBE,

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

	enum class QUEUE_TRANSITION : uint32_t
	{
		NO_TRANSITION,
		GRAPHICS,
		TRANSFER,
		COMPUTE,

		ENUM_SIZE
	};

	enum class BARRIER_PIPELINE_STAGE : uint32_t
	{
		TOP_OF_PIPELINE,
		TRANSFER,
		VERTEX_INPUT,
		VERTEX_SHADER,
		FRAGMENT_TEST,
		FRAGMENT_SHADER,
		COLOR_ATTACH_OUTPUT,
		HOST_READABLE,
		END_OF_PIPELINE,

		ENUM_SIZE
	};

	enum class BARRIER_ACCESS_MASK : uint32_t
	{
		NONE = 0,
		TRANSFER_READ,
		TRANSFER_WRITE,
		DEPTH_STENCIL_READ_WRITE,
		SHADER_READ,
		COLOR_ATTACHMENT_WRITE,
		HOST_READABLE,

		ENUM_SIZE
	};

	struct RenderingAttachmentDepth
	{
		bool load_depth;
		bool store_depth;
		IMAGE_LAYOUT image_layout;
		RImageView image_view;
		struct clearvalue
		{
			float depth = 1.0f;
			uint32_t stencil = 0;
		} clear_value;
	};

	struct RenderingAttachmentColor
	{
		bool load_color;
		bool store_color;
		IMAGE_LAYOUT image_layout;
		RImageView image_view;
		float4 clear_value_rgba;
	};

	struct StartRenderingInfo
	{
		uint2 render_area_extent;
		int2 render_area_offset;

		RenderingAttachmentDepth* depth_attachment;
		Slice<RenderingAttachmentColor> color_attachments;
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

	enum class DESCRIPTOR_TYPE : uint32_t
	{
		READONLY_CONSTANT, //CBV or uniform buffer
		READONLY_BUFFER, //SRV or Storage buffer
		READWRITE, //UAV or readwrite storage buffer(?)
		IMAGE,
		SAMPLER,

		ENUM_SIZE
	};

	struct DescriptorBindingInfo
	{
		uint32_t binding;
		uint32_t count;
		DESCRIPTOR_TYPE type;
		SHADER_STAGE shader_stage;
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
