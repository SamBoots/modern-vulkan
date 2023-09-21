#pragma once
#include "Common.h"
#include "Rendererfwd.hpp"

namespace BB
{
	namespace Render
	{
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
	}
}