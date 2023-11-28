#pragma once

#include "Common.h"
#include "Rendererfwd.hpp"
#include "Slice.h"

#include "Storage/LinkedList.h"

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
	};

	struct CreateMeshInfo
	{
		Slice<Vertex> vertices;
		Slice<uint32_t> indices;
	};

	struct CreateShaderEffectInfo
	{
		const char* name;
		const char* shader_path;
		const char* shader_entry;
		SHADER_STAGE stage;
		SHADER_STAGE_FLAGS next_stages;
		uint32_t push_constant_space;
	};

	struct CreateMaterialInfo
	{
		Slice<ShaderEffectHandle> shader_effects;
		RTexture base_color;
		RTexture normal_texture;
	};

	struct UploadImageInfo
	{
		const char* name;
		const void* pixels;
		uint32_t bit_count;
		uint32_t width;
		uint32_t height;
	};

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

	const RenderIO& BB::GetRenderIO();

	bool InitializeRenderer(StackAllocator_t& a_stack_allocator, const RendererCreateInfo& a_render_create_info);

	void StartFrame();
	void EndFrame();

	void SetView(const float4x4& a_view);
	void SetProjection(const float4x4& a_projection);

	UploadBufferView& GetUploadView(const size_t a_upload_size);
	CommandPool& GetGraphicsCommandPool();
	CommandPool& GetTransferCommandPool();

	bool ExecuteGraphicCommands(const BB::Slice<CommandPool> a_cmd_pools, const BB::Slice<UploadBufferView> a_upload_views);
	bool ExecuteTransferCommands(const BB::Slice<CommandPool> a_cmd_pools, const BB::Slice<UploadBufferView> a_upload_views);

	const MeshHandle CreateMesh(const CreateMeshInfo& a_create_info);
	void FreeMesh(const MeshHandle a_mesh);

	bool CreateShaderEffect(Allocator a_temp_allocator, const Slice<CreateShaderEffectInfo> a_create_infos, ShaderEffectHandle* a_handles);
	void FreeShaderEffect(const ShaderEffectHandle a_shader_effect);

	const MaterialHandle CreateMaterial(const CreateMaterialInfo& a_create_info);
	void FreeMaterial(const MaterialHandle a_material);

	const RTexture UploadTexture(const RCommandList a_list, const UploadImageInfo& a_upload_info, UploadBufferView& a_upload_view);
	void FreeTexture(const RTexture a_texture);

	const GPUBuffer CreateGPUBuffer(const GPUBufferCreateInfo& a_create_info);
	void FreeGPUBuffer(const GPUBuffer a_buffer);
	void* MapGPUBuffer(const GPUBuffer a_buffer);
	void UnmapGPUBuffer(const GPUBuffer a_buffer);


	void SetPushConstants(const RCommandList a_list, const RPipelineLayout a_pipe_layout, const uint32_t a_offset, const uint32_t a_size, const void* a_data);
	void StartRenderPass(const RCommandList a_list, const StartRenderingInfo& a_start_pass);
	void EndRenderPass(const RCommandList a_list, const EndRenderingInfo& a_end_pass);
	void SetScissor(const RCommandList a_list, const ScissorInfo& a_scissor);

	void DrawMesh(const MeshHandle a_mesh, const float4x4& a_transform, const uint32_t a_index_start, const uint32_t a_index_count, const MaterialHandle a_material);

	RTexture GetWhiteTexture();
	RTexture GetBlackTexture();
}
