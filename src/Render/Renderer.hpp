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

	struct UploadImageInfo
	{
		const char* name;
		const void* pixels;
		uint32_t bit_count;
		uint32_t width;
		uint32_t height;
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

		RBuffer GetBufferHandle() const { return upload_buffer_handle; }
		uint32_t UploadBufferViewOffset() const { return offset; }

	private:
		RBuffer upload_buffer_handle;	//8
		void* view_mem_start;			//16
		//I suppose we never make an upload buffer bigger then 2-4 gb? test for it on uploadbufferpool creation
		uint32_t offset;				//20
		uint32_t size;					//24
		uint32_t used;					//28
		uint32_t pool_index;			//32
		uint64_t fence_value;			//40
		//LinkedListNode holds next, so //48
	};

	void InitializeRenderer(StackAllocator_t& a_stack_allocator, const RendererCreateInfo& a_render_create_info);

	void StartFrame();
	void EndFrame();

	void SetView(const float4x4& a_view);
	void SetProjection(const float4x4& a_projection);

	UploadBufferView& GetUploadView(const size_t a_upload_size);

	const MeshHandle CreateMesh(const CreateMeshInfo& a_create_info);
	void FreeMesh(const MeshHandle a_mesh);

	const RTexture UploadTexture(const UploadImageInfo& a_upload_info, const RCommandList a_list, UploadBufferView& a_upload_view);
	void FreeTexture(const RTexture a_texture);

	void DrawMesh(const MeshHandle a_mesh, const float4x4& a_transform);
}
