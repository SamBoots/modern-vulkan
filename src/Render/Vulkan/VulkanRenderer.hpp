#pragma once
#include "Common.h"
#include "RendererTypes.hpp"
#include "Slice.h"

namespace BB
{
	namespace Render
	{
		bool InitializeVulkan(StackAllocator_t& a_stack_allocator, const char* a_app_name, const char* a_engine_name, const bool a_debug);

		bool CreateSwapchain(StackAllocator_t& a_stack_allocator, const WindowHandle a_window_handle, const uint32_t a_width, const uint32_t a_height, const uint32_t a_backbuffer_count);

		BufferView CreateVertexBuffer(const size_t a_byte_size);
		BufferView CreateIndexBuffer(const size_t a_byte_size);
	}
}