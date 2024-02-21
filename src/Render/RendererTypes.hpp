#pragma once

#include "Rendererfwd.hpp"
#include "Storage/FixedArray.h"

namespace BB
{
	using ShaderObject = FrameworkHandle<struct ShaderObjectTag>;
	using RPipeline = FrameworkHandle<struct RPipelineTag>;
	using RDescriptorLayout = FrameworkHandle<struct RDescriptorLayoutTag>;
	using RPipelineLayout = FrameworkHandle<struct RPipelineLayoutTag>;
	using RDepthBuffer = FrameworkHandle<struct RDepthBufferTag>;

	enum class QUEUE_TYPE : uint32_t
	{
		GRAPHICS,
		TRANSFER,
		COMPUTE,

		ENUM_SIZE
	};

	enum class RESOURCE_TYPE : uint32_t
	{
		DESCRIPTOR_HEAP,
		DESCRIPTOR,
		QUEUE,
		COMMAND_ALLOCATOR,
		COMMANT_LIST,
		BUFFER,
		IMAGE,
		SAMPLER,
		FENCE,

		ENUM_SIZE
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

	enum class DEPTH_FORMAT : uint32_t
	{
		D32_SFLOAT,
		D32_SFLOAT_S8_UINT,
		D24_UNORM_S8_UINT,

		ENUM_SIZE
	};

	enum class IMAGE_TILING : uint32_t
	{
		LINEAR,
		OPTIMAL,

		ENUM_SIZE
	};

	enum class PRESENT_IMAGE_RESULT
	{
		SWAPCHAIN_OUT_OF_DATE,
		SUCCESS
	};

	struct ImageCreateInfo
	{
		const char* name = nullptr;	//8
		uint32_t width = 0;			//12
		uint32_t height = 0;		//16
		uint32_t depth = 0;			//20

		uint16_t array_layers = 0;	//22
		uint16_t mip_levels = 0;	//24
		IMAGE_TYPE type{};			//28
		IMAGE_FORMAT format{};		//32
		IMAGE_TILING tiling{};		//36
		IMAGE_USAGE usage{};		//40
	};

	struct ImageViewCreateInfo
	{
		const char* name = nullptr;	//8

		RImage image;				//16
		uint16_t array_layers = 0;	//18
		uint16_t mip_levels = 0;	//20
		IMAGE_TYPE type{};			//24
		IMAGE_FORMAT format{};		//28
		bool is_depth_image = false;//32
	};

	enum class SAMPLER_ADDRESS_MODE : uint32_t
	{
		REPEAT,
		MIRROR,
		BORDER,
		CLAMP,

		ENUM_SIZE
	};

	enum class SAMPLER_FILTER : uint32_t
	{
		NEAREST,
		LINEAR
	};

	struct SamplerCreateInfo
	{
		const char* name;
		SAMPLER_ADDRESS_MODE mode_u{};
		SAMPLER_ADDRESS_MODE mode_v{};
		SAMPLER_ADDRESS_MODE mode_w{};

		SAMPLER_FILTER filter{};
		float max_anistoropy;

		float min_lod;
		float max_lod;
	};

	struct RenderDepthCreateInfo
	{
		const char* name = nullptr;
		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t depth = 0;
		DEPTH_FORMAT depth_format;
	};

	struct DescriptorBindingInfo
	{
		uint32_t binding;
		uint32_t count;
		DESCRIPTOR_TYPE type;
		SHADER_STAGE shader_stage;
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

	struct RenderCopyBufferToImageInfo
	{
		GPUBuffer src_buffer;
		uint32_t src_offset;

		RImage dst_image;
		uint3 dst_extent;
		ImageCopyInfo dst_image_info;
	};

	struct RenderCopyImageToBufferInfo
	{
		RImage src_image;
		uint3 src_extent;
		ImageCopyInfo src_image_info;

		GPUBuffer dst_buffer;
		uint32_t dst_offset;
	};

	struct RenderCopyImage
	{
		uint3 extent;
		RImage src_image;
		ImageCopyInfo src_copy_info;
		RImage dst_image;
		ImageCopyInfo dst_copy_info;
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

	struct PipelineBarrierGlobalInfo
	{
		BARRIER_PIPELINE_STAGE src_stage{};
		BARRIER_PIPELINE_STAGE dst_stage{};
		BARRIER_ACCESS_MASK src_mask{};
		BARRIER_ACCESS_MASK dst_mask{};
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
		RImage image{};						//8
		IMAGE_LAYOUT old_layout{};			//12
		IMAGE_LAYOUT new_layout{};			//16
		BARRIER_PIPELINE_STAGE src_stage{};	//20
		BARRIER_PIPELINE_STAGE dst_stage{};	//24
		BARRIER_ACCESS_MASK src_mask{};		//28
		BARRIER_ACCESS_MASK dst_mask{};		//32

		QUEUE_TRANSITION src_queue{};		//36
		QUEUE_TRANSITION dst_queue{};		//40

		uint32_t base_mip_level = 0;		//44
		uint32_t level_count = 0;			//48
		uint32_t base_array_layer = 0;		//52
		uint32_t layer_count = 0;			//56
	};

	struct PipelineBarrierInfo
	{
		uint32_t global_info_count = 0;
		const PipelineBarrierGlobalInfo* global_infos = nullptr;
		uint32_t buffer_info_count = 0;
		const PipelineBarrierBufferInfo* buffer_infos = nullptr;
		uint32_t image_info_count = 0;
		const PipelineBarrierImageInfo* image_infos = nullptr;
	};

	struct DescriptorAllocation
	{
		uint32_t size;
		uint32_t offset;
		void* buffer_start; //Maybe just get this from the descriptor heap? We only have one heap anyway.
	};

	using RQueue = FrameworkHandle<struct RQueueTag>;
	using RFence = FrameworkHandle<struct RFenceTag>;

	struct ExecuteCommandsInfo
	{
		const RCommandList* lists;
		uint32_t list_count;

		const RFence* wait_fences;
		const uint64_t* wait_values;
		uint32_t wait_count;

		const RFence* signal_fences;
		const uint64_t* signal_values;
		uint32_t signal_count;
	};

	struct PushConstantRange
	{
		SHADER_STAGE stages;
		uint32_t offset;
		uint32_t size;
	};

	struct CreatePipelineInfo
	{
		RPipelineLayout layout;
		struct ShaderCode
		{
			size_t shader_code_size;
			const void* shader_code;
			const char* shader_entry;
		};

		DEPTH_FORMAT depth_format;
		IMAGE_FORMAT rendering_format;

		ShaderCode vertex;
		ShaderCode fragment;
	};

	struct ShaderObjectCreateInfo
	{
		//maybe flags.
		SHADER_STAGE stage;
		SHADER_STAGE_FLAGS next_stages;
		size_t shader_code_size;
		const void* shader_code;
		const char* shader_entry;
		uint32_t descriptor_layout_count;
		FixedArray<RDescriptorLayout, SPACE_AMOUNT> descriptor_layouts;
		PushConstantRange push_constant_range;
	};

	struct WriteDescriptorImage
	{
		RImageView view;
		IMAGE_LAYOUT layout;
	};

	struct WriteDescriptorData
	{
		uint32_t binding;
		uint32_t descriptor_index;
		DESCRIPTOR_TYPE type{};
		union
		{
			GPUBufferView buffer_view{};
			WriteDescriptorImage image_view;
		};
	};

	struct WriteDescriptorInfos
	{
		RDescriptorLayout descriptor_layout{};
		DescriptorAllocation allocation;

		BB::Slice<WriteDescriptorData> data;
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
