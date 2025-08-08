#pragma once

#include "Common.h"
#include "Rendererfwd.hpp"
#include "Slice.h"

#include "Storage/LinkedList.h"
#include "MemoryInterfaces.hpp"
#include "Storage/FixedArray.h"
#include "BBImage.hpp"

class RenderQueue;

namespace BB
{
	// get one pool per thread
	class CommandPool : public LinkedListNode<CommandPool>
	{
		friend RenderQueue;
		uint32_t m_list_count; //4 
		uint32_t m_list_current_free; //8
		RCommandList* m_lists; //16
		uint64_t m_fence_value; //24
		RCommandPool m_api_cmd_pool; //32
		//LinkedListNode has next ptr value //40 
		bool m_recording; //44
		uint32_t pool_index; //48

		void ResetPool();
	public:
		const RCommandList* GetLists() const { return m_lists; }
		uint32_t GetListsRecorded() const { return m_list_current_free; }
 
		RCommandList StartCommandList(const char* a_name = nullptr);
		void EndCommandList(RCommandList a_list);
	};

	bool InitializeRenderer(MemoryArena& a_arena, const RendererCreateInfo& a_render_create_info);
	bool DestroyRenderer();

	void GPUWaitIdle();

	GPUDeviceInfo GetGPUInfo(MemoryArena& a_arena);
	uint32_t GetBackBufferCount();

	struct RenderStartFrameInfo
	{
		float2 mouse_pos;
		float delta_time;
	};

	void RenderStartFrame(const RCommandList a_list, const RenderStartFrameInfo& a_info, const RImage a_render_target, uint32_t& a_back_buffer_index);
	PRESENT_IMAGE_RESULT RenderEndFrame(const RCommandList a_list, const RImage a_render_target, const uint32_t a_render_target_layer);

	bool ResizeSwapchain(const uint2 a_extent);

	void StartRenderPass(const RCommandList a_list, const StartRenderingInfo& a_render_info);
	void EndRenderPass(const RCommandList a_list);

	void BindIndexBuffer(const RCommandList a_list, const uint64_t a_offset, const bool a_cpu_readable = false);
	RPipelineLayout BindShaders(const RCommandList a_list, const ShaderEffectHandle a_vertex, const ShaderEffectHandle a_fragment_pixel, const ShaderEffectHandle a_geometry);
	void SetBlendMode(const RCommandList a_list, const uint32_t a_first_attachment, const Slice<ColorBlendState> a_blend_states);
    void SetPrimitiveTopology(const RCommandList a_list, const PRIMITIVE_TOPOLOGY a_topology);
	void SetFrontFace(const RCommandList a_list, const bool a_is_clockwise);
	void SetCullMode(const RCommandList a_list, const CULL_MODE a_cull_mode);
	void SetDepthBias(const RCommandList a_list, const float a_bias_constant_factor, const float a_bias_clamp, const float a_bias_slope_factor);
	void SetScissor(const RCommandList a_list, const ScissorInfo& a_scissor);

	void DrawVertices(const RCommandList a_list, const uint32_t a_vertex_count, const uint32_t a_instance_count, const uint32_t a_first_vertex, const uint32_t a_first_instance);
	void DrawCubemap(const RCommandList a_list, const uint32_t a_instance_count, const uint32_t a_first_instance);
	void DrawIndexed(const RCommandList a_list, const uint32_t a_index_count, const uint32_t a_instance_count, const uint32_t a_first_index, const int32_t a_vertex_offset, const uint32_t a_first_instance);

	CommandPool& GetGraphicsCommandPool();
	CommandPool& GetTransferCommandPool();
	CommandPool& GetCommandCommandPool();

	PRESENT_IMAGE_RESULT PresentFrame(const BB::Slice<CommandPool> a_cmd_pools, const RFence* a_signal_fences, const uint64_t* a_signal_values, const uint32_t a_signal_count, uint64_t& a_out_present_fence_value, const bool a_skip);
	bool ExecuteGraphicCommands(const BB::Slice<CommandPool> a_cmd_pools, const RFence* a_signal_fences, const uint64_t* a_signal_values, const uint32_t a_signal_count, uint64_t& a_out_present_fence_value);
	bool ExecuteTransferCommands(const BB::Slice<CommandPool> a_cmd_pools, const RFence* a_signal_fences, const uint64_t* a_signal_values, const uint32_t a_signal_count, uint64_t& a_out_present_fence_value);
	bool ExecuteComputeCommands(const BB::Slice<CommandPool> a_cmd_pools, const RFence* a_signal_fences, const uint64_t* a_signal_values, const uint32_t a_signal_count, uint64_t& a_out_present_fence_value);

	GPUBufferView AllocateFromVertexBuffer(const size_t a_size_in_bytes);
	GPUBufferView AllocateFromIndexBuffer(const size_t a_size_in_bytes);
	void CopyToVertexBuffer(const RCommandList a_list, const GPUBuffer a_src, const Slice<RenderCopyBufferRegion> a_regions);
	void CopyToIndexBuffer(const RCommandList a_list, const GPUBuffer a_src, const Slice<RenderCopyBufferRegion> a_regions);

	WriteableGPUBufferView AllocateFromWritableVertexBuffer(const size_t a_size_in_bytes);
	WriteableGPUBufferView AllocateFromWritableIndexBuffer(const size_t a_size_in_bytes);

	bool CreateShaderEffect(MemoryArena& a_temp_arena, const Slice<CreateShaderEffectInfo> a_create_infos, ShaderEffectHandle* const a_handles, bool a_link_shaders);
	bool ReloadShaderEffect(const ShaderEffectHandle a_shader_effect, const Buffer& a_shader);

	// returns invalid texture when not enough upload buffer space
	const RImage CreateImage(const ImageCreateInfo& a_create_info);
	const RDescriptorIndex CreateImageView(const ImageViewCreateInfo& a_create_info);
	const RImageView CreateImageViewShaderInaccessible(const ImageViewCreateInfo& a_create_info);
	const RImageView GetImageView(const RDescriptorIndex a_index);
	void FreeImage(const RImage a_image);
	void FreeImageView(const RDescriptorIndex a_index);
	void FreeImageViewShaderInaccessible(const RImageView a_image_view);

	void ClearImage(const RCommandList a_list, const ClearImageInfo& a_clear_info);
	void ClearDepthImage(const RCommandList a_list, const ClearDepthImageInfo& a_clear_info);
	void BlitImage(const RCommandList a_list, const BlitImageInfo& a_blit_info);
	void CopyImage(const RCommandList a_list, const CopyImageInfo& a_copy_info);
	void CopyBufferToImage(const RCommandList a_list, const RenderCopyBufferToImageInfo& a_copy_info);
	void CopyImageToBuffer(const RCommandList a_list, const RenderCopyImageToBufferInfo& a_copy_info);

	const GPUBuffer CreateGPUBuffer(const GPUBufferCreateInfo& a_create_info);
	void FreeGPUBuffer(const GPUBuffer a_buffer);
	void* MapGPUBuffer(const GPUBuffer a_buffer);
	void UnmapGPUBuffer(const GPUBuffer a_buffer);
	GPUAddress GetGPUBufferAddress(const GPUBuffer a_buffer);
	void CopyBuffer(const RCommandList a_list, const RenderCopyBuffer& a_copy_buffer);

	size_t AccelerationStructureInstanceUploadSize();
	bool UploadAccelerationStructureInstances(void* a_mapped, const size_t a_mapped_size, const ConstSlice<AccelerationStructureInstanceInfo> a_instances);
    AccelerationStructSizeInfo GetBottomLevelAccelerationStructSizeInfo(MemoryArena& a_temp_arena, const ConstSlice<AccelerationStructGeometrySize> a_geometry_sizes, const ConstSlice<uint32_t> a_primitive_counts);
    AccelerationStructSizeInfo GetTopLevelAccelerationStructSizeInfo(MemoryArena& a_temp_arena, const ConstSlice<GPUAddress> a_instances);
	RAccelerationStruct CreateBottomLevelAccelerationStruct(const uint32_t a_acceleration_structure_size, const GPUBuffer a_dst_buffer, const uint64_t a_dst_offset);
	RAccelerationStruct CreateTopLevelAccelerationStruct(const uint32_t a_acceleration_structure_size, const GPUBuffer a_dst_buffer, const uint64_t a_dst_offset);
    GPUAddress GetAccelerationStructureAddress(const RAccelerationStruct a_acc_struct);

    void BuildBottomLevelAccelerationStruct(MemoryArena& a_temp_arena, const RCommandList a_list, const BuildBottomLevelAccelerationStructInfo& a_build_info);
    void BuildTopLevelAccelerationStruct(MemoryArena& a_temp_arena, const RCommandList a_list, const BuildTopLevelAccelerationStructInfo& a_build_info);

    void DescriptorWriteImage(const uint32_t a_descriptor_index, const RImageView a_view, const IMAGE_LAYOUT a_layout);
    void DescriptorWriteSampler(const DescriptorWriteImageInfo& a_write_info);
	void DescriptorWriteUniformBuffer(const uint32_t a_descriptor_index, const GPUBufferView& a_buffer_view);
	void DescriptorWriteStorageBuffer(const uint32_t a_descriptor_index, const GPUBufferView& a_buffer_view);

	RFence CreateFence(const uint64_t a_initial_value, const char* a_name);
	void FreeFence(const RFence a_fence);
	void WaitFence(const RFence a_fence, const GPUFenceValue a_fence_value);
	void WaitFences(const RFence* a_fences, const GPUFenceValue* a_fence_values, const uint32_t a_fence_count);
	GPUFenceValue GetCurrentFenceValue(const RFence a_fence);

	void SetPushConstants(const RCommandList a_list, const RPipelineLayout a_pipe_layout, const uint32_t a_offset, const uint32_t a_size, const void* a_data);
	void PipelineBarriers(const RCommandList a_list, const struct PipelineBarrierInfo& a_barrier_info);

	RDescriptorIndex GetDebugTexture();
}
