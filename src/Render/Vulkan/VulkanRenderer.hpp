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
		bool InitializeVulkan(MemoryArena& a_arena, const RendererCreateInfo& a_create_info);
		GPUDeviceInfo GetGPUDeviceInfo(MemoryArena& a_arena);

		bool CreateSwapchain(MemoryArena& a_arena, const WindowHandle a_window_handle, const uint32_t a_width, const uint32_t a_height, uint32_t& a_backbuffer_count);
		bool RecreateSwapchain(const uint32_t a_width, const uint32_t a_height);

		void CreateCommandPool(const QUEUE_TYPE a_queue_type, const uint32_t a_command_list_count, RCommandPool& a_pool, RCommandList* a_plists);
		void FreeCommandPool(const RCommandPool a_pool);

		const GPUBuffer CreateBuffer(const GPUBufferCreateInfo& a_create_info);
		void FreeBuffer(const GPUBuffer a_buffer);
		GPUAddress GetBufferAddress(const GPUBuffer a_buffer);

		size_t AccelerationStructureInstanceUploadSize();
		bool UploadAccelerationStructureInstances(void* a_mapped, const size_t a_mapped_size, const ConstSlice<AccelerationStructureInstanceInfo> a_instances);
		AccelerationStructSizeInfo GetBottomLevelAccelerationStructSizeInfo(MemoryArena& a_temp_arena, const ConstSlice<AccelerationStructGeometrySize> a_geometry_sizes, const ConstSlice<uint32_t> a_primitive_counts, const GPUAddress a_vertex_device_address, const GPUAddress a_index_device_address);
		AccelerationStructSizeInfo GetTopLevelAccelerationStructSizeInfo(MemoryArena& a_temp_arena, const ConstSlice<GPUAddress> a_instances);
		RAccelerationStruct CreateBottomLevelAccelerationStruct(const uint32_t a_acceleration_structure_size, const GPUBuffer a_dst_buffer, const uint64_t a_dst_offset);
		RAccelerationStruct CreateTopLevelAccelerationStruct(const uint32_t a_acceleration_structure_size, const GPUBuffer a_dst_buffer, const uint64_t a_dst_offset);
		GPUAddress GetAccelerationStructureAddress(const RAccelerationStruct a_acc_struct);

		const RImage CreateImage(const ImageCreateInfo& a_create_info);
		void FreeImage(const RImage a_image);

		const RImageView CreateImageView(const ImageViewCreateInfo& a_create_info);
		void FreeViewImage(const RImageView a_image_view);

        const RSampler CreateSampler(const SamplerCreateInfo& a_create_info);
        void FreeSampler(const RSampler a_sampler);

        void DescriptorWriteGlobal(const GPUBufferView& a_buffer_view);
        void DescriptorWriteImage(const RDescriptorIndex a_descriptor_index, const RImageView a_view, const IMAGE_LAYOUT a_layout);
        void DescriptorWriteSampler(const RDescriptorIndex a_descriptor_index, const RSampler a_sampler);
        void DescriptorWriteStorageBuffer(const RDescriptorIndex a_descriptor_index, const GPUBufferView& a_buffer_view);
        void DescriptorWriteUniformBuffer(const RDescriptorIndex a_descriptor_index, const GPUBufferView& a_buffer_view);


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
		
		void BuildBottomLevelAccelerationStruct(MemoryArena& a_temp_arena, const RCommandList a_list, const BuildBottomLevelAccelerationStructInfo& a_build_info, const GPUAddress a_vertex_device_address, const GPUAddress a_index_device_address);
		void BuildTopLevelAccelerationStruct(MemoryArena& a_temp_arena, const RCommandList a_list, const BuildTopLevelAccelerationStructInfo& a_build_info);

		void StartRenderPass(const RCommandList a_list, const StartRenderingInfo& a_render_info);
		void EndRenderPass(const RCommandList a_list);
		void SetScissor(const RCommandList a_list, const ScissorInfo& a_scissor);

		void BindIndexBuffer(const RCommandList a_list, const GPUBuffer a_buffer, const uint64_t a_offset);
		void SetPrimitiveTopology(const RCommandList a_list, const PRIMITIVE_TOPOLOGY a_topology);
		void BindShaders(const RCommandList a_list, const uint32_t a_shader_stage_count, const SHADER_STAGE* a_shader_stages, const ShaderObject* a_shader_objects);
		void SetBlendMode(const RCommandList a_list, const uint32_t a_first_attachment, const Slice<ColorBlendState> a_blend_states);
        void SetPrimitiveTopology(const RCommandList a_list, const PRIMITIVE_TOPOLOGY a_topology);
        void SetFrontFace(const RCommandList a_list, const bool a_is_clockwise);
		void SetCullMode(const RCommandList a_list, const CULL_MODE a_cull_mode);
		void SetDepthBias(const RCommandList a_list, const float a_bias_constant_factor, const float a_bias_clamp, const float a_bias_slope_factor);
		void SetPushConstants(const RCommandList a_list, const uint32_t a_offset, const uint32_t a_size, const void* a_data);

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
