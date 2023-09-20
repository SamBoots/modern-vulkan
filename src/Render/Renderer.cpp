#include "Renderer.hpp"
#include "VulkanRenderer.hpp"

using namespace BB;
using namespace Render;

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
	};
	struct IndexBuffer
	{
		RBuffer buffer;
		uint32_t size;
		uint32_t used;
	};
	VertexBuffer vertex_buffer;
	IndexBuffer index_buffer;

	//temp, to be replaced with a slotmap.
	uint32_t mesh_array_size;
	uint32_t mesh_array_pos;
	Mesh* mesh_array;
};

static RenderInterface_inst* s_render_inst;

BufferView AllocateFromVertexBuffer(const size_t a_size_in_bytes)
{
	BufferView view;

	view.buffer = s_render_inst->vertex_buffer.buffer;
	view.size = a_size_in_bytes;
	view.offset = s_render_inst->vertex_buffer.used;

	s_render_inst->vertex_buffer.used += a_size_in_bytes;

	return view;
}

BufferView AllocateFromIndexBuffer(const size_t a_size_in_bytes)
{
	BufferView view;

	view.buffer = s_render_inst->index_buffer.buffer;
	view.size = a_size_in_bytes;
	view.offset = s_render_inst->index_buffer.used;

	s_render_inst->index_buffer.used += a_size_in_bytes;

	return view;
}

void Render::InitializeRenderer(StackAllocator_t& a_stack_allocator, const RendererCreateInfo& a_render_create_info)
{
	InitializeVulkan(a_stack_allocator, a_render_create_info.app_name, a_render_create_info.engine_name, a_render_create_info.debug);
	CreateSwapchain(a_stack_allocator, a_render_create_info.window_handle, a_render_create_info.swapchain_width, a_render_create_info.swapchain_height, 3);

	s_render_inst = BBnew(a_stack_allocator, RenderInterface_inst);
	s_render_inst->swapchain_window = a_render_create_info.window_handle;
	s_render_inst->swapchain_width = a_render_create_info.swapchain_width;
	s_render_inst->swapchain_height = a_render_create_info.swapchain_height;
	s_render_inst->debug = a_render_create_info.debug;

	s_render_inst->mesh_array_size = 32;
	s_render_inst->mesh_array_pos = 0;
	s_render_inst->mesh_array = BBnewArr(a_stack_allocator, s_render_inst->mesh_array_size, Mesh);

	{
		BufferCreateInfo vertex_buffer;
		vertex_buffer.name = "global vertex buffer";
		vertex_buffer.size = mbSize * 64;
		vertex_buffer.type = BUFFER_TYPE::VERTEX;

		s_render_inst->vertex_buffer.buffer = CreateBuffer(vertex_buffer);
		s_render_inst->vertex_buffer.size = vertex_buffer.size;
		s_render_inst->vertex_buffer.used = 0;
	}
	{
		BufferCreateInfo index_buffer;
		index_buffer.name = "global index buffer";
		index_buffer.size = mbSize * 64;
		index_buffer.type = BUFFER_TYPE::INDEX;

		s_render_inst->index_buffer.buffer = CreateBuffer(index_buffer);
		s_render_inst->index_buffer.size = index_buffer.size;
		s_render_inst->index_buffer.used = 0;
	}
}

MeshHandle Render::CreateMesh(const CreateMeshInfo& a_create_info)
{
	Mesh mesh;
	mesh.vertex_buffer = AllocateFromVertexBuffer(a_create_info.vertices.sizeInBytes());
	mesh.index_buffer = AllocateFromIndexBuffer(a_create_info.vertices.sizeInBytes());

	const uint32_t mesh_pos = s_render_inst->mesh_array_pos++;
	BB_ASSERT(s_render_inst->mesh_array_pos <= s_render_inst->mesh_array_size, "no more free space for new meshes!");
	s_render_inst->mesh_array[mesh_pos] = mesh;
	return MeshHandle(mesh_pos);
}

void Render::FreeMesh(const MeshHandle a_mesh)
{
	//todo......
}

void Render::DrawMesh(const MeshHandle a_mesh, const Mat4x4& a_transform)
{

}