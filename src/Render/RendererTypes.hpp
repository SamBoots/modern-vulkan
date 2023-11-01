#pragma once

//don't care about uninitialized variable warnings
#pragma warning(suppress : 26495) 
#include "Rendererfwd.hpp"
#include "Slice.h"

namespace BB
{
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

		uint16_t array_layers = 0;
		uint16_t mip_levels = 0;
		IMAGE_TYPE type{};
		IMAGE_FORMAT format{};
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
		SHADER_STAGE next_stages;
		size_t shader_code_size;
		const void* shader_code;
		const char* shader_entry;
		uint32_t descriptor_layout_count;
		RDescriptorLayout* descriptor_layouts;
		uint32_t push_constant_range_count;
		PushConstantRange* push_constant_ranges;
	};

	struct WriteDescriptorData
	{
		uint32_t binding;
		uint32_t descriptor_index;
		DESCRIPTOR_TYPE type{};
		union
		{
			BufferView buffer_view{};
			//WriteDescriptorImage image;
		};
	};

	struct WriteDescriptorInfos
	{
		RDescriptorLayout descriptor_layout{};
		DescriptorAllocation allocation;

		BB::Slice<WriteDescriptorData> data;
	};
}

#pragma warning(default : 26495) 