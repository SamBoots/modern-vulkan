#pragma once
//shared shader include
#include "shared_common.hlsl.h"
#include "Storage/Array.h"
#include "Storage/FixedArray.h"

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

	using RPipelineLayout = FrameworkHandle<struct RPipelineLayoutTag>;
	using RDescriptorLayout = FrameworkHandle<struct RDescriptorLayoutTag>;
	using GPUFenceValue = FrameworkHandle<struct GPUFenceValueTag>;

	using GPUBuffer = FrameworkHandle<struct RBufferTag>;
	using RImage = FrameworkHandle<struct RImageTag>;
	using RImageView = FrameworkHandle<struct RImageViewTag>;

	using RFence = FrameworkHandle<struct RFenceTag>;
	
	using ShaderCode = FrameworkHandle<struct ShaderCodeTag>;
	using ShaderEffectHandle = FrameworkHandle<struct ShaderEffectHandleTag>;

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

		D16_UNORM,
		D32_SFLOAT,
		D32_SFLOAT_S8_UINT,
		D24_UNORM_S8_UINT,

		ENUM_SIZE
	};

	enum class IMAGE_ASPECT : uint32_t
	{
		COLOR,
		DEPTH, 
		DEPTH_STENCIL,

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
		TYPE_1D_ARRAY,
		TYPE_2D_ARRAY,
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
		DEPTH_STENCIL_READ_ONLY,
		SHADER_READ_ONLY,
		PRESENT,

		ENUM_SIZE
	};

	enum class IMAGE_USAGE : uint32_t
	{
		DEPTH,
		SHADOW_MAP,
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

	enum class CULL_MODE : uint32_t
	{
		NONE,
		FRONT,
		BACK,
		FRONT_AND_BACK,

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

	struct ImageSizeInfo
	{
		int32_t offset_x;
		int32_t offset_y;
		int32_t offset_z;

		uint32_t mip_level;
		uint16_t base_array_layer;
		uint16_t layer_count;
	};

	struct CopyImageInfo
	{
		uint3 extent;
		RImage src_image;
		ImageSizeInfo src_copy_info;
		RImage dst_image;
		ImageSizeInfo dst_copy_info;
	};

	struct BlitImageInfo
	{
		RImage src_image;
		int3 src_offset_p0;
		int3 src_offset_p1;
		uint32_t src_mip_level;
		uint32_t src_layer_count;
		uint32_t src_base_layer;

		RImage dst_image;
		int3 dst_offset_p0;
		int3 dst_offset_p1;
		uint32_t dst_mip_level;
		uint32_t dst_layer_count;
		uint32_t dst_base_layer;
	};

	struct DescriptorAllocation
	{
		uint32_t size;
		uint32_t offset;
		void* buffer_start; //Maybe just get this from the descriptor heap? We only have one heap anyway.
	};

	struct DescriptorWriteBufferInfo
	{
		RDescriptorLayout descriptor_layout{};
		DescriptorAllocation allocation;
		uint32_t binding;
		uint32_t descriptor_index;

		GPUBufferView buffer_view;
	};

	struct DescriptorWriteImageInfo
	{
		RDescriptorLayout descriptor_layout{};
		DescriptorAllocation allocation;
		uint32_t binding;
		uint32_t descriptor_index;

		RImageView view;
		IMAGE_LAYOUT layout;
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

	struct RenderCopyBufferRegion
	{
		uint64_t size;
		uint64_t src_offset;
		uint64_t dst_offset;
	};

	struct RenderCopyBuffer
	{
		GPUBuffer dst;
		GPUBuffer src;
		Slice<RenderCopyBufferRegion> regions;
	};

	struct Mesh
	{
		uint64_t vertex_buffer_offset;
		uint64_t index_buffer_offset;
	};

	struct PipelineBarrierGlobalInfo
	{
		BARRIER_PIPELINE_STAGE src_stage{};
		BARRIER_PIPELINE_STAGE dst_stage{};
		BARRIER_ACCESS_MASK src_mask{};
		BARRIER_ACCESS_MASK dst_mask{};
	};

	using ShaderDescriptorLayouts = FixedArray<RDescriptorLayout, SPACE_AMOUNT>;
	struct CreateShaderEffectInfo
	{
		const char* name;
		const char* shader_entry;
		Buffer shader_data;
		SHADER_STAGE stage;
		SHADER_STAGE_FLAGS next_stages;
		uint32_t push_constant_space;

		ShaderDescriptorLayouts desc_layouts;
		uint32_t desc_layout_count;
	};

	struct PipelineBarrierBufferInfo
	{
		GPUBuffer buffer{};
		uint32_t size = 0;
		uint32_t offset = 0;
		BARRIER_PIPELINE_STAGE src_stage{};
		BARRIER_PIPELINE_STAGE dst_stage{};
		BARRIER_ACCESS_MASK src_mask{};
		BARRIER_ACCESS_MASK dst_mask{};

		QUEUE_TRANSITION src_queue = QUEUE_TRANSITION::NO_TRANSITION;
		QUEUE_TRANSITION dst_queue = QUEUE_TRANSITION::NO_TRANSITION;
	};

	struct PipelineBarrierImageInfo
	{
		RImage image{};						// 8
		IMAGE_LAYOUT old_layout{};			// 12
		IMAGE_LAYOUT new_layout{};			// 16
		BARRIER_PIPELINE_STAGE src_stage{};	// 20
		BARRIER_PIPELINE_STAGE dst_stage{};	// 24
		BARRIER_ACCESS_MASK src_mask{};		// 28
		BARRIER_ACCESS_MASK dst_mask{};		// 32

		QUEUE_TRANSITION src_queue{};		// 36
		QUEUE_TRANSITION dst_queue{};		// 40

		uint32_t base_mip_level = 0;		// 44
		uint32_t level_count = 0;			// 48
		uint32_t base_array_layer = 0;		// 52
		uint32_t layer_count = 0;			// 56
		IMAGE_ASPECT aspects;				// 60
	};

	struct PipelineBarrierInfo
	{
		uint32_t global_info_count;
		const PipelineBarrierGlobalInfo* global_infos;
		uint32_t buffer_info_count;
		const PipelineBarrierBufferInfo* buffer_infos;
		uint32_t image_info_count;
		const PipelineBarrierImageInfo* image_infos;
	};

	struct ImageCreateInfo
	{
		const char* name;
		uint32_t width;
		uint32_t height;
		uint32_t depth;

		uint16_t array_layers;
		uint16_t mip_levels;
		IMAGE_TYPE type;
		IMAGE_FORMAT format;
		IMAGE_USAGE usage;
		bool use_optimal_tiling;
		bool is_cube_map;
	};

	struct ImageViewCreateInfo
	{
		const char* name;
		RImage image;
		uint16_t array_layers;
		uint16_t mip_levels;
		uint16_t base_array_layer;
		uint16_t base_mip_level;
		IMAGE_VIEW_TYPE type;
		IMAGE_FORMAT format;
		IMAGE_ASPECT aspects;
	};

	struct ClearImageInfo
	{
		RImage image;
		float4 clear_color;
		IMAGE_LAYOUT layout;
		uint32_t layer_count;
		uint32_t base_array_layer;
		uint32_t level_count;
		uint32_t base_mip_level;
	};

	struct ClearDepthImageInfo
	{
		RImage image;
		float clear_depth;
		uint32_t clear_stencil;
		IMAGE_ASPECT depth_aspects;
		IMAGE_LAYOUT layout;
		uint32_t layer_count;
		uint32_t base_array_layer;
		uint32_t level_count;
		uint32_t base_mip_level;
	};

	enum class BLEND_OP
	{
		ADD,
		SUBTRACT,

		ENUM_SIZE
	};

	enum class BLEND_MODE
	{
		FACTOR_ZERO,
		FACTOR_ONE,
		FACTOR_SRC_ALPHA,
		FACTOR_ONE_MINUS_SRC_ALPHA,
		FACTOR_DST_ALPHA,

		ENUM_SIZE
	};

	struct ColorBlendState
	{
		bool blend_enable;
		uint32_t color_flags;
		BLEND_OP color_blend_op;
		BLEND_MODE src_blend;
		BLEND_MODE dst_blend;
		BLEND_OP alpha_blend_op;
		BLEND_MODE src_alpha_blend;
		BLEND_MODE dst_alpha_blend;
	};
}
