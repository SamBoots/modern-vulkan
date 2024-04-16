#pragma once
#include "Common.h"
#include "RendererTypes.hpp"
#include "Slice.h"
#include "Storage/FixedArray.h"
#include "MemoryArena.hpp"

#define _ENABLE_REBUILD_SHADERS

namespace BB
{
	namespace Vulkan //annoying, but many function names actually overlap.
	{
		bool InitializeVulkan(MemoryArena& a_arena, const char* a_app_name, const char* a_engine_name, const bool a_debug);
		GPUDeviceInfo GetGPUDeviceInfo(MemoryArena& a_arena);

		bool CreateSwapchain(MemoryArena& a_arena, const WindowHandle a_window_handle, const uint32_t a_width, const uint32_t a_height, uint32_t& a_backbuffer_count);
		bool RecreateSwapchain(const uint32_t a_width, const uint32_t a_height);

		void CreateCommandPool(const QUEUE_TYPE a_queue_type, const uint32_t a_command_list_count, RCommandPool& a_pool, RCommandList* a_plists);
		void FreeCommandPool(const RCommandPool a_pool);

		const GPUBuffer CreateBuffer(const GPUBufferCreateInfo& a_create_info);
		void FreeBuffer(const GPUBuffer a_buffer);

		const RImage CreateImage(const ImageCreateInfo& a_create_info);
		void FreeImage(const RImage a_image);

		const RImageView CreateViewImage(const ImageViewCreateInfo& a_create_info);
		void FreeViewImage(const RImageView a_image_view);

		void CreateDepthBuffer(const RenderDepthCreateInfo& a_create_info, RImage& a_out_image, RImageView& a_out_image_view);

		RDescriptorLayout CreateDescriptorLayout(MemoryArena& a_temp_arena, Slice<DescriptorBindingInfo> a_bindings);
		RDescriptorLayout CreateDescriptorSamplerLayout(const Slice<SamplerCreateInfo> a_static_samplers);
		DescriptorAllocation AllocateDescriptor(const RDescriptorLayout a_descriptor);
		void WriteDescriptors(const WriteDescriptorInfos& a_write_info);

		RPipelineLayout CreatePipelineLayout(const RDescriptorLayout* a_descriptor_layouts, const uint32_t a_layout_count, const PushConstantRange* a_constant_ranges, const uint32_t a_constant_range_count);
		void FreePipelineLayout(const RPipelineLayout a_layout);

		RPipeline CreatePipeline(const CreatePipelineInfo& a_info);
		ShaderObject CreateShaderObject(const ShaderObjectCreateInfo& a_shader_object);
		void CreateShaderObjects(MemoryArena& a_temp_arena, Slice<ShaderObjectCreateInfo> a_shader_objects, ShaderObject* a_pshader_objects);
		void DestroyShaderObject(const ShaderObject a_shader_object);

		void* MapBufferMemory(const GPUBuffer a_buffer);
		void UnmapBufferMemory(const GPUBuffer a_buffer);

		void ResetCommandPool(const RCommandPool a_pool);
		void StartCommandList(const RCommandList a_list, const char* a_name);
		void EndCommandList(const RCommandList a_list);

		void CopyBuffer(const RCommandList a_list, const RenderCopyBuffer& a_copy_buffer);
		void CopyBuffers(const RCommandList a_list, const RenderCopyBuffer* a_copy_buffers, const uint32_t a_copy_buffer_count);
		void CopyImage(const RCommandList a_list, const RenderCopyImage& a_copy_info);
		void CopyBufferToImage(const RCommandList a_list, const RenderCopyBufferToImageInfo& a_copy_info);
		void CopyImageToBuffer(const RCommandList a_list, const RenderCopyImageToBufferInfo& a_copy_info);
		void PipelineBarriers(const RCommandList a_list, const PipelineBarrierInfo& a_BarrierInfo);
		void ClearImage(const RCommandList a_list, const RImage a_image, const IMAGE_LAYOUT a_layout, const float4 a_clear_color);
		void BlitImage(const RCommandList a_list, const BlitImageInfo& a_info);

		void StartRenderPass(const RCommandList a_list, const StartRenderingInfo& a_render_info, const RImageView a_rendering_image_view);
		void EndRenderPass(const RCommandList a_list);
		void SetScissor(const RCommandList a_list, const ScissorInfo& a_scissor);

		void BindIndexBuffer(const RCommandList a_list, const GPUBuffer a_buffer, const uint64_t a_offset);
		void BindPipeline(const RCommandList a_list, const RPipeline a_pipeline);
		void BindShaders(const RCommandList a_list, const uint32_t a_shader_stage_count, const SHADER_STAGE* a_shader_stages, const ShaderObject* a_shader_objects);
		void SetFrontFace(const RCommandList a_list, const bool a_is_clockwise);
		void SetDescriptorImmutableSamplers(const RCommandList a_list, const RPipelineLayout a_pipe_layout);
		void SetDescriptorBufferOffset(const RCommandList a_list, const RPipelineLayout a_pipe_layout, const uint32_t a_first_set, const uint32_t a_set_count, const uint32_t* a_buffer_indices, const size_t* a_offsets);
		void SetPushConstants(const RCommandList a_list, const RPipelineLayout a_pipe_layout, const uint32_t a_offset, const uint32_t a_size, const void* a_data);

		void DrawIndexed(const RCommandList a_list, const uint32_t a_index_count, const uint32_t a_instance_count, const uint32_t a_first_index, const int32_t a_vertex_offset, const uint32_t a_first_instance);
		PRESENT_IMAGE_RESULT UploadImageToSwapchain(const RCommandList a_list, const RImage a_src_image, const int2 a_src_image_size, const int2 a_swapchain_size, const uint32_t a_backbuffer_index);

		void ExecuteCommandLists(const RQueue a_queue, const ExecuteCommandsInfo* a_execute_infos, const uint32_t a_execute_info_count);
		PRESENT_IMAGE_RESULT ExecutePresentCommandList(const RQueue a_queue, const ExecuteCommandsInfo& a_execute_info, const uint32_t a_backbuffer_index);

		RFence CreateFence(const uint64_t a_initial_value, const char* a_name);
		void FreeFence(const RFence a_fence);
		void WaitFence(const RFence a_fence, const uint64_t a_fence_value);
		void WaitFences(const RFence* a_fences, const uint64_t* a_fence_values, const uint32_t a_fence_count);
		uint64_t GetCurrentFenceValue(const RFence a_fence);

		RQueue GetQueue(const QUEUE_TYPE a_queue_type, const char* a_name);
	}
}
