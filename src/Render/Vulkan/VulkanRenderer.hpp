#pragma once
#include "Common.h"
#include "RendererTypes.hpp"
#include "Slice.h"

namespace BB
{
	namespace Render
	{
		enum class BUFFER_TYPE
		{
			UPLOAD,
			STORAGE,
			UNIFORM,
			VERTEX,
			INDEX
		};

		struct BufferCreateInfo
		{
			const char* name = nullptr;
			uint64_t size = 0;
			BUFFER_TYPE type;
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

		enum class RENDER_SHADER_STAGE : uint32_t
		{
			ALL,
			VERTEX,
			FRAGMENT_PIXEL
		};

		struct DescriptorBindingInfo
		{
			uint32_t binding;
			uint32_t count;
			RENDER_DESCRIPTOR_TYPE type;
			RENDER_SHADER_STAGE shader_stage;
		};

		namespace Vulkan //annoying, but many function names actually overlap.
		{
			bool InitializeVulkan(StackAllocator_t& a_stack_allocator, const char* a_app_name, const char* a_engine_name, const bool a_debug);

			bool CreateSwapchain(StackAllocator_t& a_stack_allocator, const WindowHandle a_window_handle, const uint32_t a_width, const uint32_t a_height, uint32_t& a_backbuffer_count);

			bool StartFrame(const uint32_t a_backbuffer);
			bool EndFrame(const uint32_t a_backbuffer);

			void CreateCommandPool(const RENDER_QUEUE_TYPE a_queue_type, const uint32_t a_command_list_count, RCommandPool& a_pool, CommandList* a_plists);
			void FreeCommandPool(const RCommandPool a_pool);

			const RBuffer CreateBuffer(const BufferCreateInfo& a_create_info);
			void FreeBuffer(const RBuffer a_buffer);

			RDescriptor CreateDescriptor(Allocator a_temp_allocator, Slice<DescriptorBindingInfo> a_bindings);
			DescriptorAllocation AllocateDescriptor(const RDescriptor a_descriptor);

			void* MapBufferMemory(const RBuffer a_buffer);
			void UnmapBufferMemory(const RBuffer a_buffer);

			void ResetCommandPool(const RCommandPool a_pool);
			void StartCommandList(const RCommandList a_list, const char* a_name);
			void EndCommandList(const RCommandList a_list);

			RQueue GetQueue(const RENDER_QUEUE_TYPE a_queue_type, const char* a_name);
		}
	}
}