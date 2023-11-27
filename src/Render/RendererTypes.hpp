#pragma once

#include "Rendererfwd.hpp"
#include "Slice.h"

namespace BB
{
	using ShaderObject = FrameworkHandle<struct ShaderObjectTag>;
	using RPipeline = FrameworkHandle<struct RPipelineTag>;
	using RDescriptorLayout = FrameworkHandle<struct RDescriptorLayoutTag>;
	using RPipelineLayout = FrameworkHandle<struct RPipelineLayoutTag>;
	using RImage = FrameworkHandle<struct RImageTag>;
	using RImageView = FrameworkHandle<struct RImageViewTag>;
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

	enum class BUFFER_TYPE : uint32_t
	{
		UPLOAD,
		STORAGE,
		UNIFORM,
		VERTEX,
		INDEX,

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

	struct BufferCreateInfo
	{
		const char* name = nullptr;
		uint64_t size = 0;
		BUFFER_TYPE type{};
		bool host_writable = false;
	};

	enum class IMAGE_TILING : uint32_t
	{
		LINEAR,
		OPTIMAL,

		ENUM_SIZE
	};

	struct ImageCreateInfo
	{
		const char* name = nullptr;
		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t depth = 0;

		uint16_t array_layers = 0;
		uint16_t mip_levels = 0;
		IMAGE_TYPE type{};
		IMAGE_FORMAT format{};
		IMAGE_TILING tiling{};
	};

	struct ImageViewCreateInfo
	{
		const char* name = nullptr;

		RImage image;
		uint16_t array_layers = 0;
		uint16_t mip_levels = 0;
		IMAGE_TYPE type{};
		IMAGE_FORMAT format{};
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

	struct BufferView
	{
		RBuffer buffer;
		uint64_t size;
		uint64_t offset;
	};

	struct RenderCopyBufferRegion
	{
		uint64_t size;
		uint64_t src_offset;
		uint64_t dst_offset;
	};

	struct RenderCopyBuffer
	{
		RBuffer dst;
		RBuffer src;
		Slice<RenderCopyBufferRegion> regions;
	};

	struct ImageCopyInfo
	{
		uint32_t size_x;
		uint32_t size_y;
		uint32_t size_z;

		int32_t offset_x;
		int32_t offset_y;
		int32_t offset_z;

		uint16_t mip_level;
		uint16_t base_array_layer;
		uint16_t layer_count;

		IMAGE_LAYOUT layout;
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
		EARLY_FRAG_TEST,
		FRAGMENT_SHADER,
		END_OF_PIPELINE,

		ENUM_SIZE
	};

	enum class BARRIER_ACCESS_MASK : uint32_t
	{
		NONE = 0,
		TRANSFER_WRITE,
		DEPTH_STENCIL_READ_WRITE,
		SHADER_READ,

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
		RBuffer buffer{};
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
		RImage image{};
		IMAGE_LAYOUT old_layout{};
		IMAGE_LAYOUT new_layout{};
		BARRIER_PIPELINE_STAGE src_stage{};
		BARRIER_PIPELINE_STAGE dst_stage{};
		BARRIER_ACCESS_MASK src_mask{};
		BARRIER_ACCESS_MASK dst_mask{};

		QUEUE_TRANSITION src_queue = QUEUE_TRANSITION::NO_TRANSITION;
		QUEUE_TRANSITION dst_queue = QUEUE_TRANSITION::NO_TRANSITION;

		uint32_t base_mip_level = 0;
		uint32_t level_count = 0;
		uint32_t base_array_layer = 0;
		uint32_t layer_count = 0;
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

	struct RenderCopyBufferToImageInfo
	{
		RBuffer src_buffer;
		uint32_t src_offset;

		RImage dst_image;
		ImageCopyInfo dst_image_info;
	};

	struct DescriptorAllocation
	{
		RDescriptorLayout descriptor;
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

	struct StartRenderingInfo
	{
		uint32_t viewport_width;
		uint32_t viewport_height;

		bool load_color;
		bool store_color;
		IMAGE_LAYOUT initial_layout;
		IMAGE_LAYOUT final_layout;

		RDepthBuffer depth_buffer{};

		float4 clear_color_rgba;
	};

	struct EndRenderingInfo
	{
		IMAGE_LAYOUT initial_layout;
		IMAGE_LAYOUT final_layout;
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
		RDescriptorLayout* descriptor_layouts;
		uint32_t push_constant_range_count;
		PushConstantRange* push_constant_ranges;
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
			BufferView buffer_view{};
			WriteDescriptorImage image_view;
		};
	};

	struct WriteDescriptorInfos
	{
		RDescriptorLayout descriptor_layout{};
		DescriptorAllocation allocation;

		BB::Slice<WriteDescriptorData> data;
	};
}
