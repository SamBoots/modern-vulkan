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

		const RImageView CreateImageView(const ImageViewCreateInfo& a_create_info);
		void FreeViewImage(const RImageView a_image_view);

		RDescriptorLayout CreateDescriptorLayout(MemoryArena& a_temp_arena, const ConstSlice<DescriptorBindingInfo> a_bindings);
		RDescriptorLayout CreateDescriptorSamplerLayout(const Slice<SamplerCreateInfo> a_static_samplers);
		DescriptorAllocation AllocateDescriptor(const RDescriptorLayout a_descriptor);
		void DescriptorWriteUniformBuffer(const DescriptorWriteBufferInfo& a_write_info);
		void DescriptorWriteStorageBuffer(const DescriptorWriteBufferInfo& a_write_info);
		void DescriptorWriteImage(const DescriptorWriteImageInfo& a_write_info);

		RPipelineLayout CreatePipelineLayout(const RDescriptorLayout* a_descriptor_layouts, const uint32_t a_layout_count, const PushConstantRange a_constant_range);
		void FreePipelineLayout(const RPipelineLayout a_layout);

		ShaderObject CreateShaderObject(const ShaderObjectCreateInfo& a_shader_object);
		void CreateShaderObjects(MemoryArena& a_temp_arena, Slice<ShaderObjectCreateInfo> a_shader_objects, ShaderObject* a_pshader_objects, const bool a_link_shaders);
		void DestroyShaderObject(const ShaderObject a_shader_object);

		void* MapBufferMemory(const GPUBuffer a_buffer);
		void UnmapBufferMemory(const GPUBuffer a_buffer);

		void ResetCommandPool(const RCommandPool a_pool);
		void StartCommandList(const RCommandList a_list, const char* a_name);
		void EndCommandList(const RCommandList a_list);

		void CopyBuffer(const RCommandList a_list, const RenderCopyBuffer& a_copy_buffer);
		void CopyImage(const RCommandList a_list, const CopyImageInfo& a_copy_info);
		void CopyBufferToImage(const RCommandList a_list, const RenderCopyBufferToImageInfo& a_copy_info);
		void CopyImageToBuffer(const RCommandList a_list, const RenderCopyImageToBufferInfo& a_copy_info);
		void PipelineBarriers(const RCommandList a_list, const PipelineBarrierInfo& a_barrier);
		void ClearImage(const RCommandList a_list, const ClearImageInfo& a_clear_info);
		void ClearDepthImage(const RCommandList a_list, const ClearDepthImageInfo& a_clear_info);
		void BlitImage(const RCommandList a_list, const BlitImageInfo& a_info);
		
		void StartRenderPass(const RCommandList a_list, const StartRenderingInfo& a_render_info);
		void EndRenderPass(const RCommandList a_list);
		void SetScissor(const RCommandList a_list, const ScissorInfo& a_scissor);

		void BindIndexBuffer(const RCommandList a_list, const GPUBuffer a_buffer, const uint64_t a_offset);
		void SetPrimitiveTopology(const RCommandList a_list, const PRIMITIVE_TOPOLOGY a_topology);
		void BindShaders(const RCommandList a_list, const uint32_t a_shader_stage_count, const SHADER_STAGE* a_shader_stages, const ShaderObject* a_shader_objects);
		void SetBlendMode(const RCommandList a_list, const uint32_t a_first_attachment, const Slice<ColorBlendState> a_blend_states);
		void SetFrontFace(const RCommandList a_list, const bool a_is_clockwise);
		void SetCullMode(const RCommandList a_list, const CULL_MODE a_cull_mode);
		void SetDepthBias(const RCommandList a_list, const float a_bias_constant_factor, const float a_bias_clamp, const float a_bias_slope_factor);
		void SetDescriptorImmutableSamplers(const RCommandList a_list, const RPipelineLayout a_pipe_layout);
		void SetDescriptorBufferOffset(const RCommandList a_list, const RPipelineLayout a_pipe_layout, const uint32_t a_first_set, const uint32_t a_set_count, const uint32_t* a_buffer_indices, const size_t* a_offsets);
		void SetPushConstants(const RCommandList a_list, const RPipelineLayout a_pipe_layout, const uint32_t a_offset, const uint32_t a_size, const void* a_data);

		void DrawVertices(const RCommandList a_list, const uint32_t a_vertex_count, const uint32_t a_instance_count, const uint32_t a_first_vertex, const uint32_t a_first_instance);
		void DrawIndexed(const RCommandList a_list, const uint32_t a_index_count, const uint32_t a_instance_count, const uint32_t a_first_index, const int32_t a_vertex_offset, const uint32_t a_first_instance);
		PRESENT_IMAGE_RESULT UploadImageToSwapchain(const RCommandList a_list, const RImage a_src_image, const uint32_t a_array_layer, const int2 a_src_image_size, const int2 a_swapchain_size, const uint32_t a_backbuffer_index);

		void ExecuteCommandLists(const RQueue a_queue, const ExecuteCommandsInfo* a_execute_infos, const uint32_t a_execute_info_count);
		PRESENT_IMAGE_RESULT ExecutePresentCommandList(const RQueue a_queue, const ExecuteCommandsInfo& a_execute_info, const uint32_t a_backbuffer_index);

		RFence CreateFence(const uint64_t a_initial_value, const char* a_name);
		void FreeFence(const RFence a_fence);
		void WaitFence(const RFence a_fence, const GPUFenceValue a_fence_value);
		void WaitFences(const RFence* a_fences, const GPUFenceValue* a_fence_values, const uint32_t a_fence_count);
		GPUFenceValue GetCurrentFenceValue(const RFence a_fence);

		RQueue GetQueue(const QUEUE_TYPE a_queue_type, const char* a_name);
	}
}
