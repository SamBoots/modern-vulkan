#pragma once

#include "Rendererfwd.hpp"
#include "Storage/FixedArray.h"

namespace BB
{
	using ShaderObject = FrameworkHandle<struct ShaderObjectTag>;

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

	using RQueue = FrameworkHandle<struct RQueueTag>;

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
		uint32_t size; // zero for no push constant
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
}
