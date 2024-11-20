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
	struct RendererCreateInfo
	{
		WindowHandle window_handle;
		const char* app_name;
		const char* engine_name;
		uint32_t swapchain_width;
		uint32_t swapchain_height;
		bool debug;

		// EXTRA STUFF
		size_t frame_upload_buffer_size = mbSize * 64;
		size_t asset_upload_buffer_size = gbSize * 1;
	};

	struct CreateMeshInfo
	{
		Slice<Vertex> vertices;
		Slice<uint32_t> indices;
	};

	struct WriteImageInfo
	{
		RImage image;
		uint2 extent;
		int2 offset;
		uint16_t layer_count;
		uint16_t base_array_layer;
		IMAGE_FORMAT format;
		const void* pixels;
		bool set_shader_visible;
	};

	constexpr IMAGE_FORMAT RENDER_TARGET_IMAGE_FORMAT = IMAGE_FORMAT::RGBA8_SRGB; // due to screenshots this is now RGBA8_SGRB, should be RGBA16_SFLOAT

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

	const RenderIO& GetRenderIO();

	bool InitializeRenderer(MemoryArena& a_arena, const RendererCreateInfo& a_render_create_info);
	bool DestroyRenderer();
	void RequestResize();

	void GPUWaitIdle();

	GPUDeviceInfo GetGPUInfo(MemoryArena& a_arena);

	struct StartFrameInfo
	{
		float2 mouse_pos;
		float delta_time;
	};

	void StartFrame(const RCommandList a_list, const StartFrameInfo& a_info, uint32_t& a_out_back_buffer_index);
	void EndFrame(const RCommandList a_list, const ShaderEffectHandle a_imgui_vertex, const ShaderEffectHandle a_imgui_fragment, const uint32_t a_back_buffer_index, bool a_skip = false);

	void StartRenderPass(const RCommandList a_list, const StartRenderingInfo& a_render_info);
	void EndRenderPass(const RCommandList a_list);
	RPipelineLayout BindShaders(const RCommandList a_list, const Slice<const ShaderEffectHandle> a_shader_effects);
	void SetFrontFace(const RCommandList a_list, const bool a_is_clockwise);
	void SetCullMode(const RCommandList a_list, const CULL_MODE a_cull_mode);
	void SetDepthBias(const RCommandList a_list, const float a_bias_constant_factor, const float a_bias_clamp, const float a_bias_slope_factor);
	void SetScissor(const RCommandList a_list, const ScissorInfo& a_scissor);

	void DrawVertices(const RCommandList a_list, const uint32_t a_vertex_count, const uint32_t a_instance_count, const uint32_t a_first_vertex, const uint32_t a_first_instance);
	void DrawCubemap(const RCommandList a_list, const uint32_t a_instance_count, const uint32_t a_first_instance);
	void DrawIndexed(const RCommandList a_list, const uint32_t a_index_count, const uint32_t a_instance_count, const uint32_t a_first_index, const int32_t a_vertex_offset, const uint32_t a_first_instance);

	CommandPool& GetGraphicsCommandPool();
	CommandPool& GetTransferCommandPool();

	bool PresentFrame(const BB::Slice<CommandPool> a_cmd_pools, const RFence* a_signal_fences, const uint64_t* a_signal_values, const uint32_t a_signal_count, uint64_t& a_out_present_fence_value);
	bool ExecuteGraphicCommands(const BB::Slice<CommandPool> a_cmd_pools, const RFence* a_signal_fences, const uint64_t* a_signal_values, const uint32_t a_signal_count, uint64_t& a_out_present_fence_value);
	
	GPUBufferView AllocateFromVertexBuffer(const size_t a_size_in_bytes);
	GPUBufferView AllocateFromIndexBuffer(const size_t a_size_in_bytes);

	WriteableGPUBufferView AllocateFromWritableVertexBuffer(const size_t a_size_in_bytes);
	WriteableGPUBufferView AllocateFromWritableIndexBuffer(const size_t a_size_in_bytes);

	// returns invalid mesh when not enough upload buffer space
	// maybe do this on the engine side, and upload later
	const Mesh CreateMesh(const CreateMeshInfo& a_create_info);
	void FreeMesh(const Mesh a_mesh);

	RDescriptorLayout CreateDescriptorLayout(MemoryArena& a_temp_arena, Slice<DescriptorBindingInfo> a_bindings);
	DescriptorAllocation AllocateDescriptor(const RDescriptorLayout a_descriptor);

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
	GPUFenceValue WriteTexture(const WriteImageInfo& a_write_info);
	GPUFenceValue ReadTexture(const RImage a_image, const IMAGE_LAYOUT a_current_layout, const uint2 a_extent, const int2 a_offset, const GPUBuffer a_readback_buffer, const size_t a_readback_buffer_size);

	GPUFenceValue GetTransferFenceValue();

	const GPUBuffer CreateGPUBuffer(const GPUBufferCreateInfo& a_create_info);
	void FreeGPUBuffer(const GPUBuffer a_buffer);
	void* MapGPUBuffer(const GPUBuffer a_buffer);
	void UnmapGPUBuffer(const GPUBuffer a_buffer);
	void CopyBuffer(const RCommandList a_list, const RenderCopyBuffer& a_copy_buffer);

	void DescriptorWriteUniformBuffer(const DescriptorWriteBufferInfo& a_write_info);
	void DescriptorWriteStorageBuffer(const DescriptorWriteBufferInfo& a_write_info);
	void DescriptorWriteImage(const DescriptorWriteImageInfo& a_write_info);

	RFence CreateFence(const uint64_t a_initial_value, const char* a_name);
	void FreeFence(const RFence a_fence);
	void WaitFence(const RFence a_fence, const uint64_t a_fence_value);
	void WaitFences(const RFence* a_fences, const uint64_t* a_fence_values, const uint32_t a_fence_count);
	uint64_t GetCurrentFenceValue(const RFence a_fence);

	void SetPushConstants(const RCommandList a_list, const RPipelineLayout a_pipe_layout, const uint32_t a_offset, const uint32_t a_size, const void* a_data);
	void PipelineBarriers(const RCommandList a_list, const struct PipelineBarrierInfo& a_barrier_info);
	void SetDescriptorBufferOffset(const RCommandList a_list, const RPipelineLayout a_pipe_layout, const uint32_t a_first_set, const uint32_t a_set_count, const uint32_t* a_buffer_indices, const size_t* a_offsets);
	const DescriptorAllocation& GetGlobalDescriptorAllocation();

	RDescriptorIndex GetWhiteTexture();
	RDescriptorIndex GetBlackTexture();
	RDescriptorIndex GetDebugTexture();

	// should always be placed as layout 0
	RDescriptorLayout GetStaticSamplerDescriptorLayout();
	// should always be placed as layout 1
	RDescriptorLayout GetGlobalDescriptorLayout();
}
