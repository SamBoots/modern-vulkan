#pragma once
#include "Rendererfwd.hpp"
#include "Slice.h"

namespace BB
{
	enum class RENDER_QUEUE_TYPE : uint32_t
	{
		GRAPHICS,
		TRANSFER,
		COMPUTE,

		ENUM_SIZE
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

	enum class BUFFER_TYPE
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

	struct BufferCreateInfo
	{
		const char* name = nullptr;
		uint64_t size = 0;
		BUFFER_TYPE type;
		bool host_writable;
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

		//RImageHandle depthStencil{};

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