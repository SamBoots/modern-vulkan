#pragma once

#include "Common.h"
#include "Rendererfwd.hpp"
#include "Slice.h"

#include "Storage/LinkedList.h"

namespace BB
{
	//get one pool per thread
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

	class UploadBufferView : public LinkedListNode<UploadBufferView>
	{
		friend class UploadBufferPool;
	public:
		bool AllocateAndMemoryCopy(const void* a_src, const uint32_t a_byte_size, uint32_t& a_out_allocation_offset)
		{
			a_out_allocation_offset = used + offset;
			const uint32_t new_used = used + a_byte_size;
			if (new_used > size) //not enough space, fail the allocation
				return false;

			memcpy(Pointer::Add(view_mem_start, used), a_src, a_byte_size);
			used = new_used;
			return true;
		}

		GPUBuffer GetBufferHandle() const { return upload_buffer_handle; }
		uint32_t UploadBufferViewOffset() const { return offset; }

	private:
		GPUBuffer upload_buffer_handle;	//8
		void* view_mem_start;			//16
		//I suppose we never make an upload buffer bigger then 2-4 gb? test for it on uploadbufferpool creation
		uint32_t offset;				//20
		uint32_t size;					//24
		uint32_t used;					//28
		uint32_t pool_index;			//32
		uint64_t fence_value;			//40
		//LinkedListNode holds next, so //48
	};

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

	const GPUBuffer CreateGPUBuffer(const GPUBufferCreateInfo& a_create_info);
	void FreeGPUBuffer(const GPUBuffer a_buffer);
	void* MapGPUBuffer(const GPUBuffer a_buffer);
	void UnmapGPUBuffer(const GPUBuffer a_buffer);

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
