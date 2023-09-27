#pragma once
#include "Common.h"
#include "RendererTypes.hpp"
#include "Slice.h"

namespace BB
{
	namespace Render
	{
		namespace Vulkan //annoying, but many function names actually overlap.
		{
			bool InitializeVulkan(StackAllocator_t& a_stack_allocator, const char* a_app_name, const char* a_engine_name, const bool a_debug);

			bool CreateSwapchain(StackAllocator_t& a_stack_allocator, const WindowHandle a_window_handle, const uint32_t a_width, const uint32_t a_height, uint32_t& a_backbuffer_count);

			void CreateCommandPool(const RENDER_QUEUE_TYPE a_queue_type, const uint32_t a_command_list_count, RCommandPool& a_pool, RCommandList* a_plists);
			void FreeCommandPool(const RCommandPool a_pool);

			const RBuffer CreateBuffer(const BufferCreateInfo& a_create_info);
			void FreeBuffer(const RBuffer a_buffer);

			RDescriptorLayout CreateDescriptorLayout(Allocator a_temp_allocator, Slice<DescriptorBindingInfo> a_bindings);
			DescriptorAllocation AllocateDescriptor(const RDescriptorLayout a_descriptor);
			RPipelineLayout CreatePipelineLayout(const RDescriptorLayout* a_descriptor_layouts, const uint32_t a_layout_count, const PushConstantRanges* a_constant_ranges, const uint32_t a_constant_range_count);
			void FreePipelineLayout(const RPipelineLayout a_layout);

			void CreateShaderObject(Allocator a_temp_allocator, Slice<ShaderObjectCreateInfo> a_shader_objects, ShaderObject* a_pshader_objects);
			void DestroyShaderObject(const ShaderObject a_shader_object);

			void* MapBufferMemory(const RBuffer a_buffer);
			void UnmapBufferMemory(const RBuffer a_buffer);

			void ResetCommandPool(const RCommandPool a_pool);
			void StartCommandList(const RCommandList a_list, const char* a_name);
			void EndCommandList(const RCommandList a_list);

			void StartRendering(const RCommandList a_list, const StartRenderingInfo& a_render_info, const uint32_t a_backbuffer_index);
			void EndRendering(const RCommandList a_list, const EndRenderingInfo& a_rendering_info, const uint32_t a_backbuffer_index);
			void BindShaders(const RCommandList a_list, const uint32_t a_shader_stage_count, const SHADER_STAGE* a_shader_stages, const ShaderObject* a_shader_objects);


			void ExecuteCommandLists(const RQueue a_queue, const ExecuteCommandsInfo* a_execute_infos, const uint32_t a_execute_info_count);
			void ExecutePresentCommandList(const RQueue a_queue, const ExecuteCommandsInfo& a_execute_info, const uint32_t a_backbuffer_index);

			bool StartFrame(const uint32_t a_backbuffer_index);
			bool EndFrame(const uint32_t a_backbuffer_index);

			RFence CreateFence(const uint64_t a_initial_value, const char* a_name);
			void FreeFence(const RFence a_fence);
			void WaitFences(const RFence* a_fences, const uint64_t* a_fence_values, const uint32_t a_fence_count);

			RQueue GetQueue(const RENDER_QUEUE_TYPE a_queue_type, const char* a_name);
		}
	}
}