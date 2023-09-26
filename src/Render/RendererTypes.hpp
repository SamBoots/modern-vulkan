#pragma once
#include "Common.h"
#include "Rendererfwd.hpp"

namespace BB
{
	namespace Render
	{
		enum class RENDER_QUEUE_TYPE : uint32_t
		{
			GRAPHICS,
			TRANSFER,
			COMPUTE
		};

		enum class RENDER_RESOURCE_TYPE : uint32_t
		{
			DESCRIPTOR_HEAP,
			DESCRIPTOR,
			QUEUE,
			COMMAND_ALLOCATOR,
			COMMANT_LIST,
			BUFFER,
			IMAGE,
			SAMPLER,
			FENCE
		};

		enum class RENDER_IMAGE_LAYOUT : uint32_t
		{
			UNDEFINED,
			GENERAL,
			TRANSFER_SRC,
			TRANSFER_DST,
			COLOR_ATTACHMENT_OPTIMAL,
			DEPTH_STENCIL_ATTACHMENT,
			SHADER_READ_ONLY,
			PRESENT
		};

		enum class RENDER_SHADER_STAGE : uint32_t
		{
			ALL,
			VERTEX,
			FRAGMENT_PIXEL
		};

		enum class BUFFER_TYPE
		{
			UPLOAD,
			STORAGE,
			UNIFORM,
			VERTEX,
			INDEX
		};

		enum class RENDER_DESCRIPTOR_TYPE : uint32_t
		{
			READONLY_CONSTANT, //CBV or uniform buffer
			READONLY_BUFFER, //SRV or Storage buffer
			READWRITE, //UAV or readwrite storage buffer(?)
			IMAGE,
			SAMPLER,
			ENUM_SIZE
		};

		struct BufferCreateInfo
		{
			const char* name = nullptr;
			uint64_t size = 0;
			BUFFER_TYPE type;
		};

		struct DescriptorBindingInfo
		{
			uint32_t binding;
			uint32_t count;
			RENDER_DESCRIPTOR_TYPE type;
			RENDER_SHADER_STAGE shader_stage;
		};

		struct BufferView
		{
			RBuffer buffer;
			uint64_t size;
			uint64_t offset;
		};

		struct DescriptorAllocation
		{
			RDescriptorLayout descriptor;
			uint32_t size;
			uint32_t offset;
		};

		using RQueue = FrameworkHandle<struct RQueueTag>;
		using RFence = FrameworkHandle<struct RFenceTag>;

		struct ExecuteCommandsInfo
		{
			const CommandList* lists;
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
			RENDER_IMAGE_LAYOUT initial_layout;
			RENDER_IMAGE_LAYOUT final_layout;

			//RImageHandle depthStencil{};

			float4 clear_color_rgba;
		};

		struct EndRenderingInfo
		{
			RENDER_IMAGE_LAYOUT initial_layout;
			RENDER_IMAGE_LAYOUT final_layout;
		};

		struct PushConstantRanges
		{
			RENDER_SHADER_STAGE stages;
			uint32_t offset;
			uint32_t size;
		};

		struct ShaderObjectCreateInfo
		{
			//maybe flags.
			RENDER_SHADER_STAGE stage;
			RENDER_SHADER_STAGE next_stages;
			size_t shader_code_size;
			const void* shader_code;
			const char* shader_entry;
			uint32_t descriptor_layout_count;
			RDescriptorLayout* descriptor_layouts;
			uint32_t push_constant_range_count;
			PushConstantRanges* push_constant_ranges;
		};
	}
}