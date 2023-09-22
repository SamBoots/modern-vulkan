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

		struct BufferView
		{
			RBuffer buffer;
			uint64_t size;
			uint64_t offset;
		};

		struct DescriptorAllocation
		{
			RDescriptor descriptor;
			uint32_t size;
			uint32_t offset;
		};

		using RQueue = FrameworkHandle<struct RQueueTag>;
		using RFence = FrameworkHandle<struct RFenceTag>;
	}
}