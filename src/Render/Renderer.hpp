#pragma once

#include "Common.h"
#include "Rendererfwd.hpp"
#include "Slice.h"

#include "Storage/LinkedList.h"
#include "MemoryInterfaces.hpp"

#include "BBImage.hpp"

namespace BB
{
	enum class RENDER_PASS_TYPE
	{
		STANDARD_3D
	};

	struct CreateShaderEffectInfo
	{
		const char* name;
		const char* shader_entry;
		Buffer shader_data;
		SHADER_STAGE stage;
		SHADER_STAGE_FLAGS next_stages;
		uint32_t push_constant_space;
		RENDER_PASS_TYPE pass_type;
	};

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

	struct WriteTextureInfo
	{
		uint2 extent;
		int2 offset;
		uint16_t layer_count;
		uint16_t base_array_layer;
		const void* pixels;
		bool set_shader_visible;
	};

	constexpr IMAGE_FORMAT RENDER_TARGET_IMAGE_FORMAT = IMAGE_FORMAT::RGBA8_SRGB; // due to screenshots this is now RGBA8_SGRB, should be RGBA16_SFLOAT
	struct CreateTextureInfo
	{
		const char* name;
		IMAGE_FORMAT format;
		IMAGE_USAGE usage;
		uint32_t width;
		uint32_t height;
		uint16_t array_layers;
	};

	// get one pool per thread
	class CommandPool : public LinkedListNode<CommandPool>
	{
		friend class RenderQueue;
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

	struct CreateLightInfo
	{
		LIGHT_TYPE light_type;      // 4
		float3 color;               // 16
		float3 pos;                 // 28
		float specular_strength;    // 32

		float radius_constant;      // 36
		float radius_linear;        // 40
		float radius_quadratic;     // 44

		float3 spotlight_direction; // 56
		float cutoff_radius;        // 60
	};

	const RenderIO& GetRenderIO();

	bool InitializeRenderer(MemoryArena& a_arena, const RendererCreateInfo& a_render_create_info);
	void RequestResize();

	void GPUWaitIdle();

	GPUDeviceInfo GetGPUInfo(MemoryArena& a_arena);

	struct StartFrameInfo
	{
		float2 mouse_pos;
		float delta_time;
	};

	void StartFrame(const RCommandList a_list, const StartFrameInfo& a_info);
	void EndFrame(const RCommandList a_list, const ShaderEffectHandle a_imgui_vertex, const ShaderEffectHandle a_imgui_fragment, bool a_skip = false);

	void StartRenderPass(const RCommandList a_list, const StartRenderingInfo& a_render_info);
	void EndRenderPass(const RCommandList a_list);
	RPipelineLayout BindShaders(const RCommandList a_list, const Slice<ShaderEffectHandle> a_shader_effects);
	void SetFrontFace(const RCommandList a_list, const bool a_is_clockwise);
	void SetCullMode(const RCommandList a_list, const CULL_MODE a_cull_mode);
	void SetClearColor(const RCommandList a_list, const float3 a_clear_color);
	void SetScissor(const RCommandList a_list, const ScissorInfo& a_scissor);

	void DrawMesh(const RCommandList a_list, const MeshHandle a_mesh, const uint32_t a_index_start, const uint32_t a_index_count);
	void DrawVertices(const RCommandList a_list, const uint32_t a_vertex_count, const uint32_t a_instance_count, const uint32_t a_first_vertex, const uint32_t a_first_instance);
	void DrawCubemap(const RCommandList a_list, const uint32_t a_instance_count, const uint32_t a_first_instance);
	void DrawIndexed(const RCommandList a_list, const uint32_t a_index_count, const uint32_t a_instance_count, const uint32_t a_first_index, const int32_t a_vertex_offset, const uint32_t a_first_instance);

	CommandPool& GetGraphicsCommandPool();
	CommandPool& GetTransferCommandPool();

	bool PresentFrame(const BB::Slice<CommandPool> a_cmd_pools, uint64_t& a_out_fence_value);
	bool ExecuteGraphicCommands(const BB::Slice<CommandPool> a_cmd_pools, uint64_t& a_out_fence_value);
	
	GPUBufferView AllocateFromVertexBuffer(const size_t a_size_in_bytes);
	GPUBufferView AllocateFromIndexBuffer(const size_t a_size_in_bytes);

	WriteableGPUBufferView AllocateFromWritableVertexBuffer(const size_t a_size_in_bytes);
	WriteableGPUBufferView AllocateFromWritableIndexBuffer(const size_t a_size_in_bytes);

	// returns invalid mesh when not enough upload buffer space
	const MeshHandle CreateMesh(const CreateMeshInfo& a_create_info);
	void FreeMesh(const MeshHandle a_mesh);

	RDescriptorLayout CreateDescriptorLayout(MemoryArena& a_temp_arena, Slice<DescriptorBindingInfo> a_bindings);
	DescriptorAllocation AllocateDescriptor(const RDescriptorLayout a_descriptor);

	bool CreateShaderEffect(MemoryArena& a_temp_arena, const Slice<CreateShaderEffectInfo> a_create_infos, ShaderEffectHandle* const a_handles);
	bool ReloadShaderEffect(const ShaderEffectHandle a_shader_effect, const Buffer& a_shader);

	struct BlitTextureInfo
	{
		RTexture src;
		int2 src_point_0;
		int2 src_point_1;
		bool src_set_shader_visible;

		RTexture dst;
		int2 dst_point_0;
		int2 dst_point_1;
		bool dst_set_shader_visible;
	};

	struct CopyTextureInfo
	{
		uint3 extent;
		RTexture src;
		bool src_set_shader_visible;
		ImageCopyInfo src_copy_info;
		RTexture dst;
		bool dst_set_shader_visible;
		ImageCopyInfo dst_copy_info;
	};

	// returns invalid texture when not enough upload buffer space
	const RTexture CreateTexture(const CreateTextureInfo& a_create_info);
	const RTexture CreateTextureCubeMap(const CreateTextureInfo& a_create_info);
	void BlitTexture(const RCommandList a_list, const BlitTextureInfo& a_blit_info);
	void CopyTexture(const RCommandList a_list, const CopyTextureInfo& a_copy_info);
	const GPUFenceValue WriteTexture(const RTexture a_texture, const WriteTextureInfo& a_write_info);
	// Hacky shit to get image/view. Change plz
	const RImage GetImage(const RTexture a_texture);
	const RImageView GetImageView(const RTexture a_texture, const uint32_t a_view_index);
	void FreeTexture(const RTexture a_texture);
	GPUFenceValue ReadTexture(const RTexture a_texture, const uint2 a_extent, const int2 a_offset, const GPUBuffer a_readback_buffer, const size_t a_readback_buffer_size);

	GPUFenceValue GetTransferFenceValue();

	const GPUBuffer CreateGPUBuffer(const GPUBufferCreateInfo& a_create_info);
	void FreeGPUBuffer(const GPUBuffer a_buffer);
	void* MapGPUBuffer(const GPUBuffer a_buffer);
	void UnmapGPUBuffer(const GPUBuffer a_buffer);
	void CopyBuffer(const RCommandList a_list, const RenderCopyBuffer& a_copy_buffer);

	void DescriptorWriteUniformBuffer(const DescriptorWriteBufferInfo& a_write_info);
	void DescriptorWriteStorageBuffer(const DescriptorWriteBufferInfo& a_write_info);
	void DescriptorWriteImageBuffer(const DescriptorWriteImageInfo& a_write_info);

	RFence CreateFence(const uint64_t a_initial_value, const char* a_name);
	void FreeFence(const RFence a_fence);
	void WaitFence(const RFence a_fence, const uint64_t a_fence_value);
	void WaitFences(const RFence* a_fences, const uint64_t* a_fence_values, const uint32_t a_fence_count);
	uint64_t GetCurrentFenceValue(const RFence a_fence);

	void SetPushConstants(const RCommandList a_list, const RPipelineLayout a_pipe_layout, const uint32_t a_offset, const uint32_t a_size, const void* a_data);
	void PipelineBarriers(const RCommandList a_list, const struct PipelineBarrierInfo& a_barrier_info);
	void SetDescriptorBufferOffset(const RCommandList a_list, const RPipelineLayout a_pipe_layout, const uint32_t a_first_set, const uint32_t a_set_count, const uint32_t* a_buffer_indices, const size_t* a_offsets);
	const DescriptorAllocation& GetGlobalDescriptorAllocation();

	RTexture GetWhiteTexture();
	RTexture GetBlackTexture();
	RTexture GetDebugTexture();
}
