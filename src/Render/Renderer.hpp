#pragma once
#include "Common.h"
#include "Rendererfwd.hpp"
#include "Slice.h"

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

	struct UploadBufferView
	{
		friend class UploadBufferPool;
		RBuffer upload_buffer_handle;	//8
		void* view_mem_start;			//16
		//I suppose we never make an upload buffer bigger then 2-4 gb? test for it on uploadbufferpool creation
		uint32_t offset;				//20
		uint32_t size;					//24
		uint64_t fence_value;			//32
		UploadBufferView* next;			//40

		void MemoryCopy(void* a_src, const uint32_t a_byte_offset, const uint32_t a_byte_size)
		{
			memcpy(Pointer::Add(view_mem_start, a_byte_offset), a_src, a_byte_size);
		}
	};

	void InitializeRenderer(StackAllocator_t& a_stack_allocator, const RendererCreateInfo& a_render_create_info);

	void StartFrame();
	void EndFrame();

	void SetView(const float4x4& a_view);
	void SetProjection(const float4x4& a_projection);

	MeshHandle CreateMesh(const CreateMeshInfo& a_create_info);
	void FreeMesh(const MeshHandle a_mesh);

	void DrawMesh(const MeshHandle a_mesh, const float4x4& a_transform);
}