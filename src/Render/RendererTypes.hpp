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

	using RQueue = FrameworkHandle<struct RQueueTag>;

	struct ExecuteCommandsInfo
	{
		const RCommandList* lists;
		uint32_t list_count;

		const RFence* wait_fences;
		const uint64_t* wait_values;
		uint32_t wait_count;
        const PIPELINE_STAGE* wait_stages;

		const RFence* signal_fences;
		const uint64_t* signal_values;
		uint32_t signal_count;
	};

	struct ShaderObjectCreateInfo
	{
        const char* name;
		//maybe flags.
		SHADER_STAGE stage;
		SHADER_STAGE_FLAGS next_stages;
		size_t shader_code_size;
		const void* shader_code;
		const char* shader_entry;
	};
}
