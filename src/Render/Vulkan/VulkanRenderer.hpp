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
			void WriteDescriptors(const WriteDescriptorInfos& a_write_info);

			RPipelineLayout CreatePipelineLayout(const RDescriptorLayout* a_descriptor_layouts, const uint32_t a_layout_count, const PushConstantRanges* a_constant_ranges, const uint32_t a_constant_range_count);
			void FreePipelineLayout(const RPipelineLayout a_layout);

			RPipeline CreatePipeline(Allocator a_temp_allocator, const CreatePipelineInfo& a_info);
			void CreateShaderObject(Allocator a_temp_allocator, Slice<ShaderObjectCreateInfo> a_shader_objects, ShaderObject* a_pshader_objects);
			void DestroyShaderObject(const ShaderObject a_shader_object);

			void* MapBufferMemory(const RBuffer a_buffer);
			void UnmapBufferMemory(const RBuffer a_buffer);

			void ResetCommandPool(const RCommandPool a_pool);
			void StartCommandList(const RCommandList a_list, const char* a_name);
			void EndCommandList(const RCommandList a_list);

			void StartRendering(const RCommandList a_list, const StartRenderingInfo& a_render_info, const uint32_t a_backbuffer_index);
			void EndRendering(const RCommandList a_list, const EndRenderingInfo& a_rendering_info, const uint32_t a_backbuffer_index);
			
			void BindVertexBuffer(const RCommandList a_list, const RBuffer a_buffer, const uint64_t a_offset);
			void BindIndexBuffer(const RCommandList a_list, const RBuffer a_buffer, const uint64_t a_offset);
			void BindPipeline(const RCommandList a_list, const RPipeline a_pipeline);
			void BindShaders(const RCommandList a_list, const uint32_t a_shader_stage_count, const SHADER_STAGE* a_shader_stages, const ShaderObject* a_shader_objects);
			void SetDescriptorBufferOffset(const RCommandList a_list, const RPipelineLayout a_pipe_layout, const uint32_t a_first_set, const uint32_t a_set_count, const uint32_t* a_buffer_indices, const size_t* a_offsets);

			void DrawIndexed(const RCommandList a_list, const uint32_t a_index_count, const uint32_t a_instance_count, const uint32_t a_first_index, const int32_t a_vertex_offset, const uint32_t a_first_instance);

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