#pragma once

#include "Common.h"
#include "RenderBackendTypes.hpp"
#include "Slice.h"

namespace BB
{
	const RenderIO& GetRenderIO();

	void StartFrame();
	void EndFrame();

	UploadBufferView& GetUploadView(const size_t a_upload_size);
	CommandPool& GetGraphicsCommandPool();
	CommandPool& GetTransferCommandPool();

	bool ExecuteGraphicCommands(const BB::Slice<CommandPool> a_cmd_pools, const BB::Slice<UploadBufferView> a_upload_views);
	bool ExecuteTransferCommands(const BB::Slice<CommandPool> a_cmd_pools, const BB::Slice<UploadBufferView> a_upload_views);

	GPUBufferView AllocateFromVertexBuffer(const size_t a_size_in_bytes);
	GPUBufferView AllocateFromIndexBuffer(const size_t a_size_in_bytes);

	WriteableGPUBufferView AllocateFromWritableVertexBuffer(const size_t a_size_in_bytes);
	WriteableGPUBufferView AllocateFromWritableIndexBuffer(const size_t a_size_in_bytes);

	//garbage code, maybe fix this.
	const RTexture BackendUploadTexture(const RCommandList a_list, const UploadImageInfo& a_upload_info, class UploadBufferView& a_upload_view);
	void BackendFreeTexture(const RTexture a_texture);

	const GPUBuffer CreateGPUBuffer(const GPUBufferCreateInfo& a_create_info);
	void FreeGPUBuffer(const GPUBuffer a_buffer);
	void* MapGPUBuffer(const GPUBuffer a_buffer);
	void UnmapGPUBuffer(const GPUBuffer a_buffer);
	void CopyBuffer(const RCommandList a_list, const RenderCopyBuffer& a_copy_buffer);
	void CopyBuffers(const RCommandList a_list, const RenderCopyBuffer* a_copy_buffers, const uint32_t a_copy_buffer_count);
	void CopyBufferImage(const RCommandList a_list, const RenderCopyBufferToImageInfo& a_copy_info);

	void BindIndexBuffer(const RCommandList a_list, const GPUBuffer a_buffer, const uint64_t a_offset);
	void BindShaderEffects(const RCommandList a_list, const uint32_t a_shader_stage_count, const ShaderEffectHandle* a_shader_objects);
	void SetPushConstants(const RCommandList a_list, const ShaderEffectHandle a_first_shader_handle, const uint32_t a_offset, const uint32_t a_size, const void* a_data);
	void StartRenderPass(const RCommandList a_list, const StartRenderingInfo& a_start_pass);
	void EndRenderPass(const RCommandList a_list);
	void SetScissor(const RCommandList a_list, const ScissorInfo& a_scissor);
	void DrawIndexed(const RCommandList a_list, const uint32_t a_index_count, const uint32_t a_instance_count, const uint32_t a_first_index, const int32_t a_vertex_offset, const uint32_t a_first_instance);

	RTexture GetWhiteTexture();
	RTexture GetBlackTexture();
}
