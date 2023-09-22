#include "Renderer.hpp"
#include "VulkanRenderer.hpp"

#include "Storage/Slotmap.h"
#include "Program.h"

using namespace BB;
using namespace Render;

struct RenderFence
{
	uint64_t next_fence_value;
	uint64_t last_complete_value;
	RFence fence;
};

//get one per thread
class CommandPool
{
	friend class RenderQueue;
	uint32_t m_list_count; //4 
	uint32_t m_list_current_free; //8
	CommandList* m_lists; //16
	uint64_t m_fence_value; //24
	RCommandPool m_api_cmd_pool; //32
	CommandPool* next; //40 
	bool m_recording; //44
	BB_PAD(4); //48
public:
	CommandList* StartCommandList(const char* a_name = nullptr)
	{
		BB_ASSERT(m_recording == false, "already recording a commandlist from this commandpool!");
		CommandList* pcmd_list = &m_lists[m_list_current_free++];
		Vulkan::StartCommandList(pcmd_list->api_cmd_list, a_name);
		m_recording = true;
		return pcmd_list;
	}

	void EndCommandList(CommandList* a_list)
	{
		BB_ASSERT(m_recording == true, "trying to end a commandlist while the pool is not recording any list");
		BB_ASSERT(a_list->api_cmd_list == m_lists[m_list_count - 1].api_cmd_list, "commandlist that was submitted is not from this pool or was already closed!");
		Vulkan::EndCommandList(a_list->api_cmd_list);
		m_recording = false;
	}
};

//THREAD SAFE: TRUE
class RenderQueue
{
public:

	RenderQueue(Allocator a_system_allocator, const RENDER_QUEUE_TYPE a_queue_type, const char* a_name, const uint32_t a_command_pool_count, const uint32_t a_command_lists_per_pool)
		:	m_pool_count(a_command_pool_count)
	{
		m_queue = Vulkan::GetQueue(a_queue_type, a_name);

		m_pools = BBnewArr(a_system_allocator, a_command_pool_count, CommandPool);
		for (uint32_t i = 0; i < a_command_pool_count; i++)
		{
			m_pools[i].m_recording = false;
			m_pools[i].m_fence_value = 0;
			m_pools[i].m_list_count = a_command_lists_per_pool;
			m_pools[i].m_list_current_free = 0;
			m_pools[i].m_lists = BBnewArr(a_system_allocator, m_pools[i].m_list_count, CommandList);
			Vulkan::CreateCommandPool(a_queue_type, a_command_lists_per_pool, m_pools[i].m_api_cmd_pool, m_pools[i].m_lists);
		}

		for (uint32_t i = 0; i < a_command_pool_count - 1; i++)
		{
			m_pools[i].next = &m_pools[i + 1];
		}

		m_pools[a_command_pool_count - 1].next = nullptr;
		m_free_pools = &m_pools[0];
		m_lock = OSCreateRWLock();
	}
	~RenderQueue()
	{
		WaitIdle();

		for (uint32_t i = 0; i < m_pool_count; i++)
		{
			Vulkan::FreeCommandPool(m_pools[i].m_api_cmd_pool);
		}
	}

	CommandPool* GetCommandPool(const char* a_pool_name = "")
	{
		OSAcquireSRWLockWrite(&m_lock);
		CommandPool* pool = BB_SLL_POP(m_free_pools);
		OSReleaseSRWLockWrite(&m_lock);
		return pool;
	}

	//void ExecuteCommands(CommandList* a_CommandLists, const uint32_t a_CommandListCount, const RenderFence* a_WaitFences, const RENDER_PIPELINE_STAGE* a_WaitStages, const uint32_t a_FenceCount);
	//void ExecutePresentCommands(CommandList* a_CommandLists, const uint32_t a_CommandListCount, const RenderFence* a_WaitFences, const RENDER_PIPELINE_STAGE* a_WaitStages, const uint32_t a_FenceCount);
	//void WaitFenceValue(const uint64_t a_FenceValue);
	void WaitIdle()
	{

	}

	RenderFence GetFence() const { return m_fence; }
	uint64_t GetNextFenceValue() const { return m_fence.next_fence_value; }
	uint64_t GetLastCompletedValue() const { return m_fence.last_complete_value; }

private:
	BBRWLock m_lock;
	const uint32_t m_pool_count;
	CommandPool* m_pools;
	CommandPool* m_free_pools;
	CommandPool* m_in_flight_pools = nullptr;

	RQueue m_queue;
	RenderFence m_fence;
};

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

		m_buffer = Vulkan::CreateBuffer(t_UploadBufferInfo);

		m_offset = 0;
		m_start = Vulkan::MapBufferMemory(m_buffer);
	}
	~UploadBuffer()
	{
		Vulkan::UnmapBufferMemory(m_buffer);
		Vulkan::FreeBuffer(m_buffer);
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

	uint32_t draw_list_count;
	uint32_t draw_list_max;
	DrawList* draw_lists;
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
	Vulkan::InitializeVulkan(a_stack_allocator, a_render_create_info.app_name, a_render_create_info.engine_name, a_render_create_info.debug);
	s_render_inst = BBnew(a_stack_allocator, RenderInterface_inst) {};
	s_render_inst->backbuffer_count = 3;
	s_render_inst->backbuffer_pos = 0;
	Vulkan::CreateSwapchain(a_stack_allocator, a_render_create_info.window_handle, a_render_create_info.swapchain_width, a_render_create_info.swapchain_height, s_render_inst->backbuffer_count);
	s_render_inst->frames = BBnewArr(a_stack_allocator, s_render_inst->backbuffer_count, RenderInterface_inst::Frame);


	BufferCreateInfo per_frame_buffer_info;
	per_frame_buffer_info.name = "per_frame_buffer";
	per_frame_buffer_info.size = mbSize * 4;
	per_frame_buffer_info.type = BUFFER_TYPE::UNIFORM;
	for (uint32_t i = 0; i < s_render_inst->backbuffer_count; i++)
	{
		s_render_inst->frames[i].per_frame_buffer.buffer = Vulkan::CreateBuffer(per_frame_buffer_info);
		s_render_inst->frames[i].per_frame_buffer.size = static_cast<uint32_t>(per_frame_buffer_info.size);
		s_render_inst->frames[i].per_frame_buffer.used = 0;
	}

	s_render_inst->swapchain_window = a_render_create_info.window_handle;
	s_render_inst->swapchain_width = a_render_create_info.swapchain_width;
	s_render_inst->swapchain_height = a_render_create_info.swapchain_height;
	s_render_inst->debug = a_render_create_info.debug;

	s_render_inst->draw_list_max = 128;
	s_render_inst->draw_list_count = 0;
	s_render_inst->draw_lists = BBnewArr(a_stack_allocator, s_render_inst->draw_list_max, DrawList);
	s_render_inst->mesh_map.Init(a_stack_allocator, 32);

	{
		BufferCreateInfo vertex_buffer;
		vertex_buffer.name = "global vertex buffer";
		vertex_buffer.size = mbSize * 64;
		vertex_buffer.type = BUFFER_TYPE::VERTEX;

		s_render_inst->vertex_buffer.buffer = Vulkan::CreateBuffer(vertex_buffer);
		s_render_inst->vertex_buffer.size = static_cast<uint32_t>(vertex_buffer.size);
		s_render_inst->vertex_buffer.used = 0;
	}
	{
		BufferCreateInfo index_buffer;
		index_buffer.name = "global index buffer";
		index_buffer.size = mbSize * 64;
		index_buffer.type = BUFFER_TYPE::INDEX;

		s_render_inst->index_buffer.buffer = Vulkan::CreateBuffer(index_buffer);
		s_render_inst->index_buffer.size = static_cast<uint32_t>(index_buffer.size);
		s_render_inst->index_buffer.used = 0;
	}
}

void  Render::StartFrame()
{
	//clear the drawlist, maybe do this on a per-frame basis of we do GPU upload?
	s_render_inst->draw_list_count = 0;

	//setup rendering
	Vulkan::StartFrame(s_render_inst->backbuffer_pos);
}

void  Render::EndFrame()
{
	//render


	//present
	Vulkan::EndFrame(s_render_inst->backbuffer_pos);

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
	s_render_inst->mesh_map.erase(a_mesh.handle);
}

void Render::DrawMesh(const MeshHandle a_mesh, const Mat4x4& a_transform)
{
	DrawList draw_list;
	draw_list.mesh = a_mesh;
	draw_list.matrix = a_transform;
	s_render_inst->draw_lists[s_render_inst->draw_list_count++] = draw_list;
}