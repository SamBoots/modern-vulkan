#include "Renderer.hpp"
#include "VulkanRenderer.hpp"

#include "Storage/Slotmap.h"

using namespace BB;
using namespace Render;

//transform this into a pool for multithread goodness
class UploadBuffer
{
public:
	struct Chunk
	{
		void* memory;
		uint32_t offset;
		uint32_t size;
	};

	UploadBuffer(const size_t a_size, const char* a_Name = nullptr)
		:	m_size(static_cast<uint32_t>(a_size))
	{
		BB_ASSERT(a_size < UINT32_MAX, "upload buffer size is larger then UINT32_MAX, this is not supported");
		BufferCreateInfo t_UploadBufferInfo{};
		t_UploadBufferInfo.name = a_Name;
		t_UploadBufferInfo.size = m_size;
		t_UploadBufferInfo.type = BUFFER_TYPE::UPLOAD;

		m_buffer = CreateBuffer(t_UploadBufferInfo);

		m_offset = 0;
		m_start = MapBufferMemory(m_buffer);
	}
	~UploadBuffer()
	{
		UnmapBufferMemory(m_buffer);
		FreeBuffer(m_buffer);
	}

	const Chunk Alloc(const size_t a_size)
	{
		BB_ASSERT(a_size < UINT32_MAX, "upload buffer alloc size is larger then UINT32_MAX, this is not supported");
		BB_ASSERT(m_size >= m_offset + a_size, "Now enough space to alloc in the uploadbuffer.");
		Chunk chunk{};
		chunk.memory = Pointer::Add(m_start, m_offset);
		chunk.offset = m_offset;
		chunk.size = static_cast<uint32_t>(a_size);
		m_offset += static_cast<uint32_t>(a_size);
		return chunk;
	}
	void Clear()
	{
		memset(m_start, 0, m_offset);
		m_offset = 0;
	}

	inline const uint32_t GetCurrentOffset() const { return m_offset; }
	inline const RBuffer Buffer() const { return m_buffer; }
	inline void* GetStart() const { return m_start; }

private:
	RBuffer m_buffer;
	const uint32_t m_size;
	uint32_t m_offset;
	void* m_start;
};

struct Mesh
{
	BufferView vertex_buffer;
	BufferView index_buffer;
};

struct DrawList
{
	MeshHandle mesh;
	Mat4x4 matrix;
};

struct RenderInterface_inst
{
	WindowHandle swapchain_window;
	uint32_t swapchain_width;
	uint32_t swapchain_height;
	bool debug;

	struct VertexBuffer
	{
		RBuffer buffer;
		uint32_t size;
		uint32_t used;
	} vertex_buffer;
	struct IndexBuffer
	{
		RBuffer buffer;
		uint32_t size;
		uint32_t used;
	} index_buffer;

	uint32_t backbuffer_count;
	uint32_t backbuffer_pos;
	struct Frame
	{
		struct UniformBuffer
		{
			RBuffer buffer;
			uint32_t size;
			uint32_t used;
		} per_frame_buffer;
		UploadBuffer upload_buffer{ mbSize * 4 };
		uint64_t fence_value;
	} *frames;

	StaticSlotmap<Mesh> mesh_map{};
};

static RenderInterface_inst* s_render_inst;

BufferView AllocateFromVertexBuffer(const size_t a_size_in_bytes)
{
	BufferView view;

	view.buffer = s_render_inst->vertex_buffer.buffer;
	view.size = a_size_in_bytes;
	view.offset = s_render_inst->vertex_buffer.used;

	s_render_inst->vertex_buffer.used += static_cast<uint32_t>(a_size_in_bytes);

	return view;
}

BufferView AllocateFromIndexBuffer(const size_t a_size_in_bytes)
{
	BufferView view;

	view.buffer = s_render_inst->index_buffer.buffer;
	view.size = a_size_in_bytes;
	view.offset = s_render_inst->index_buffer.used;

	s_render_inst->index_buffer.used += static_cast<uint32_t>(a_size_in_bytes);

	return view;
}

void Render::InitializeRenderer(StackAllocator_t& a_stack_allocator, const RendererCreateInfo& a_render_create_info)
{
	InitializeVulkan(a_stack_allocator, a_render_create_info.app_name, a_render_create_info.engine_name, a_render_create_info.debug);
	s_render_inst = BBnew(a_stack_allocator, RenderInterface_inst) {};
	s_render_inst->backbuffer_count = 3;
	s_render_inst->backbuffer_pos = 0;
	CreateSwapchain(a_stack_allocator, a_render_create_info.window_handle, a_render_create_info.swapchain_width, a_render_create_info.swapchain_height, s_render_inst->backbuffer_count);
	s_render_inst->frames = BBnewArr(a_stack_allocator, s_render_inst->backbuffer_count, RenderInterface_inst::Frame);


	BufferCreateInfo per_frame_buffer_info;
	per_frame_buffer_info.name = "per_frame_buffer";
	per_frame_buffer_info.size = mbSize * 4;
	per_frame_buffer_info.type = BUFFER_TYPE::UNIFORM;
	for (uint32_t i = 0; i < s_render_inst->backbuffer_count; i++)
	{
		s_render_inst->frames[i].per_frame_buffer.buffer = CreateBuffer(per_frame_buffer_info);
		s_render_inst->frames[i].per_frame_buffer.size = static_cast<uint32_t>(per_frame_buffer_info.size);
		s_render_inst->frames[i].per_frame_buffer.used = 0;
	}

	s_render_inst->swapchain_window = a_render_create_info.window_handle;
	s_render_inst->swapchain_width = a_render_create_info.swapchain_width;
	s_render_inst->swapchain_height = a_render_create_info.swapchain_height;
	s_render_inst->debug = a_render_create_info.debug;

	s_render_inst->mesh_map.Init(a_stack_allocator, 32);

	{
		BufferCreateInfo vertex_buffer;
		vertex_buffer.name = "global vertex buffer";
		vertex_buffer.size = mbSize * 64;
		vertex_buffer.type = BUFFER_TYPE::VERTEX;

		s_render_inst->vertex_buffer.buffer = CreateBuffer(vertex_buffer);
		s_render_inst->vertex_buffer.size = static_cast<uint32_t>(vertex_buffer.size);
		s_render_inst->vertex_buffer.used = 0;
	}
	{
		BufferCreateInfo index_buffer;
		index_buffer.name = "global index buffer";
		index_buffer.size = mbSize * 64;
		index_buffer.type = BUFFER_TYPE::INDEX;

		s_render_inst->index_buffer.buffer = CreateBuffer(index_buffer);
		s_render_inst->index_buffer.size = static_cast<uint32_t>(index_buffer.size);
		s_render_inst->index_buffer.used = 0;
	}
}

void  Render::StartFrame()
{
	//setup rendering
	VulkanStartFrame(s_render_inst->backbuffer_pos);
}

void  Render::EndFrame()
{
	//render


	//present
	VulkanEndFrame(s_render_inst->backbuffer_pos);

	//swap images
	s_render_inst->backbuffer_pos = (s_render_inst->backbuffer_pos + 1) % s_render_inst->backbuffer_count;
	s_render_inst->backbuffer_pos;
}

MeshHandle Render::CreateMesh(const CreateMeshInfo& a_create_info)
{
	Mesh mesh;
	mesh.vertex_buffer = AllocateFromVertexBuffer(a_create_info.vertices.sizeInBytes());
	mesh.index_buffer = AllocateFromIndexBuffer(a_create_info.vertices.sizeInBytes());

	return MeshHandle(s_render_inst->mesh_map.insert(mesh).handle);
}

void Render::FreeMesh(const MeshHandle a_mesh)
{
	//todo......
}

void Render::DrawMesh(const MeshHandle a_mesh, const Mat4x4& a_transform)
{

}