#include "Renderer.hpp"
#include "VulkanRenderer.hpp"

#include "Storage/Slotmap.h"
#include "Program.h"

#include "ShaderCompiler.h"

using namespace BB;
using namespace Render;

struct RenderFence
{
	uint64_t next_fence_value;
	uint64_t last_complete_value;
	RFence fence;
};

//get one pool per thread
class CommandPool
{
	friend class RenderQueue;
	uint32_t m_list_count; //4 
	uint32_t m_list_current_free; //8
	RCommandList* m_lists; //16
	uint64_t m_fence_value; //24
	RCommandPool m_api_cmd_pool; //32
	CommandPool* next; //40 
	bool m_recording; //44
	BB_PAD(4); //48
public:
	CommandList StartCommandList(const char* a_name = nullptr)
	{
		BB_ASSERT(m_recording == false, "already recording a commandlist from this commandpool!");
		BB_ASSERT(m_list_current_free < m_list_count, "command pool out of lists!");
		CommandList list{ m_lists[m_list_current_free++] };
		Vulkan::StartCommandList(list.api_cmd_list, a_name);
		m_recording = true;
		return list;
	}

	void EndCommandList(CommandList a_list)
	{
		BB_ASSERT(m_recording == true, "trying to end a commandlist while the pool is not recording any list");
		BB_ASSERT(a_list.api_cmd_list == m_lists[m_list_current_free - 1], "commandlist that was submitted is not from this pool or was already closed!");
		Vulkan::EndCommandList(a_list.api_cmd_list);
		m_recording = false;
	}
	void ResetPool()
	{
		BB_ASSERT(m_recording == false, "trying to reset a pool while still recording");
		Vulkan::ResetCommandPool(m_api_cmd_pool);
		m_list_current_free = 0;
	}
};

//THREAD SAFE: TRUE
class RenderQueue
{
public:

	RenderQueue(Allocator a_system_allocator, const RENDER_QUEUE_TYPE a_queue_type, const char* a_name, const uint32_t a_command_pool_count, const uint32_t a_command_lists_per_pool)
		:	m_pool_count(a_command_pool_count)
	{
		m_queue_type = a_queue_type;
		m_queue = Vulkan::GetQueue(a_queue_type, a_name);
		m_fence.fence = Vulkan::CreateFence(0, "make_it_queue_name_sam!!!");
		m_fence.last_complete_value = 0;
		m_fence.next_fence_value = 1;

		m_pools = BBnewArr(a_system_allocator, a_command_pool_count, CommandPool);
		for (uint32_t i = 0; i < a_command_pool_count; i++)
		{
			m_pools[i].m_recording = false;
			m_pools[i].m_fence_value = 0;
			m_pools[i].m_list_count = a_command_lists_per_pool;
			m_pools[i].m_list_current_free = 0;
			m_pools[i].m_lists = BBnewArr(a_system_allocator, m_pools[i].m_list_count, RCommandList);
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
		Vulkan::FreeFence(m_fence.fence);
		for (uint32_t i = 0; i < m_pool_count; i++)
		{
			Vulkan::FreeCommandPool(m_pools[i].m_api_cmd_pool);
		}
	}

	CommandPool* GetCommandPool(const char* a_pool_name = "")
	{
		OSAcquireSRWLockWrite(&m_lock);
		BB_ASSERT(m_free_pools, "pool is a nullptr, no more pools left!");
		CommandPool* pool = BB_SLL_POP(m_free_pools);
		OSReleaseSRWLockWrite(&m_lock);
		return pool;
	}

	void ReturnPool(CommandPool* a_pool)
	{
		OSAcquireSRWLockWrite(&m_lock);
		a_pool->m_fence_value = m_fence.next_fence_value;
		BB_SLL_PUSH(m_in_flight_pools, a_pool);
		OSReleaseSRWLockWrite(&m_lock);
	}

	void ExecuteCommands(CommandList* a_lists, const uint32_t a_list_count, const RFence* const a_wait_fences, const uint64_t* const wait_values, const uint32_t a_fence_count)
	{
		ExecuteCommandsInfo execute_info;
		execute_info.lists = a_lists;
		execute_info.list_count = a_list_count;
		execute_info.wait_fences = a_wait_fences;
		execute_info.wait_values = wait_values;
		execute_info.wait_count = a_fence_count;
		execute_info.signal_fences = &m_fence.fence;
		execute_info.signal_values = &m_fence.next_fence_value;
		execute_info.signal_count = 1;

		OSAcquireSRWLockWrite(&m_lock);
		Vulkan::ExecuteCommandLists(m_queue, &execute_info, 1);
		++m_fence.next_fence_value;
		OSReleaseSRWLockWrite(&m_lock);
	}

	void ExecutePresentCommands(CommandList* a_lists, const uint32_t a_list_count, const RFence* const a_wait_fences, const uint64_t* const wait_values, const uint32_t a_fence_count, const uint32_t a_backbuffer_index)
	{
		BB_ASSERT(m_queue_type == RENDER_QUEUE_TYPE::GRAPHICS, "calling a present commands on a non-graphics command queue is not valid");
		ExecuteCommandsInfo execute_info;
		execute_info.lists = a_lists;
		execute_info.list_count = a_list_count;
		execute_info.wait_fences = a_wait_fences;
		execute_info.wait_values = wait_values;
		execute_info.wait_count = a_fence_count;
		execute_info.signal_fences = &m_fence.fence;
		execute_info.signal_values = &m_fence.next_fence_value;
		execute_info.signal_count = 1;

		OSAcquireSRWLockWrite(&m_lock);
		Vulkan::ExecutePresentCommandList(m_queue, execute_info, a_backbuffer_index);
		++m_fence.next_fence_value;
		OSReleaseSRWLockWrite(&m_lock);
	}

	//void ExecutePresentCommands(CommandList* a_CommandLists, const uint32_t a_CommandListCount, const RenderFence* a_WaitFences, const RENDER_PIPELINE_STAGE* a_WaitStages, const uint32_t a_FenceCount);
	void WaitFenceValue(const uint64_t a_FenceValue)
	{
		OSAcquireSRWLockWrite(&m_lock);

		Vulkan::WaitFences(&m_fence.fence, &a_FenceValue, 1);

		//Thank you Descent Raytracer teammates great code that I can steal
		for (CommandPool** in_flight_command_polls = &m_in_flight_pools; *in_flight_command_polls;)
		{
			CommandPool* command_pool = *in_flight_command_polls;

			if (command_pool->m_fence_value <= a_FenceValue)
			{
				command_pool->ResetPool();

				//Get next in-flight commandlist
				*in_flight_command_polls = command_pool->next;
				BB_SLL_PUSH(m_free_pools, command_pool);
			}
			else
			{
				in_flight_command_polls = &command_pool->next;
			}
		}

		OSReleaseSRWLockWrite(&m_lock);
	}

	void WaitIdle()
	{
		WaitFenceValue(m_fence.next_fence_value - 1);
	}

	RenderFence GetFence() const { return m_fence; }
	uint64_t GetNextFenceValue() const { return m_fence.next_fence_value; }
	uint64_t GetLastCompletedValue() const { return m_fence.last_complete_value; }

private:
	RENDER_QUEUE_TYPE m_queue_type; //4
	BBRWLock m_lock; //12
	const uint32_t m_pool_count; //16
	CommandPool* m_pools; //24
	CommandPool* m_free_pools; //32 
	CommandPool* m_in_flight_pools = nullptr; //40

	RQueue m_queue;//48 
	RenderFence m_fence; //72
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
	DescriptorAllocation mesh_descriptor_allocation;
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
	RenderInterface_inst(Allocator a_system_allocator)
		: graphics_queue(a_system_allocator, RENDER_QUEUE_TYPE::GRAPHICS, "graphics queue", 8, 8)
	{}

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

	RenderQueue graphics_queue;
};

static RenderInterface_inst* s_render_inst;

CommandPool* current_use_pool;
CommandList current_command_list;

ShaderObject vertex_object;
ShaderObject fragment_object;
RDescriptorLayout vertex_descriptor_layout;
RPipelineLayout pipeline_layout;

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
	s_render_inst = BBnew(a_stack_allocator, RenderInterface_inst)(a_stack_allocator);
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

	InitShaderCompiler();

	BBStackAllocatorScope(a_stack_allocator)
	{
		//temp stuff
		DescriptorBindingInfo descriptor_bindings[1];
		descriptor_bindings[0].binding = 0;
		descriptor_bindings[0].count = 1;
		descriptor_bindings[0].shader_stage = SHADER_STAGE::VERTEX;
		descriptor_bindings[0].type = RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER;
		vertex_descriptor_layout = Vulkan::CreateDescriptorLayout(a_stack_allocator, Slice(descriptor_bindings, 1));

		pipeline_layout = Vulkan::CreatePipelineLayout(&vertex_descriptor_layout, 1, nullptr, 0);

		const ShaderCode vertex_shader = CompileShader(a_stack_allocator, "../resources/shaders/hlsl/Debug.hlsl", "VertexMain", SHADER_STAGE::VERTEX);
		const ShaderCode fragment_shader = CompileShader(a_stack_allocator, "../resources/shaders/hlsl/Debug.hlsl", "FragmentMain", SHADER_STAGE::FRAGMENT_PIXEL);

		Buffer shader_buffer = GetShaderCodeBuffer(vertex_shader);
		ShaderObjectCreateInfo shader_objects_info[2];
		shader_objects_info[0].stage = SHADER_STAGE::VERTEX;
		shader_objects_info[0].next_stages = SHADER_STAGE::FRAGMENT_PIXEL;
		shader_objects_info[0].shader_code_size = shader_buffer.size;
		shader_objects_info[0].shader_code = shader_buffer.data;
		shader_objects_info[0].shader_entry = "VertexMain";
		shader_objects_info[0].descriptor_layout_count = 0;
		shader_objects_info[0].push_constant_range_count = 0;

		shader_buffer = GetShaderCodeBuffer(fragment_shader);
		shader_objects_info[1].stage = SHADER_STAGE::FRAGMENT_PIXEL;
		shader_objects_info[1].next_stages = SHADER_STAGE::NONE;
		shader_objects_info[1].shader_code_size = shader_buffer.size;
		shader_objects_info[1].shader_code = shader_buffer.data;
		shader_objects_info[1].shader_entry = "FragmentMain";
		shader_objects_info[1].descriptor_layout_count = 1;
		shader_objects_info[1].descriptor_layouts = &vertex_descriptor_layout;
		shader_objects_info[1].push_constant_range_count = 0;

		ShaderObject shader_objects[2];
		Vulkan::CreateShaderObject(a_stack_allocator, Slice(shader_objects_info, _countof(shader_objects_info)), shader_objects);

		vertex_object = shader_objects[0];
		fragment_object = shader_objects[1];

		ReleaseShaderCode(vertex_shader);
		ReleaseShaderCode(fragment_shader);
	}
}

void  Render::StartFrame()
{
	s_render_inst->graphics_queue.WaitFenceValue(s_render_inst->frames[s_render_inst->backbuffer_pos].fence_value);

	//clear the drawlist, maybe do this on a per-frame basis of we do GPU upload?
	s_render_inst->draw_list_count = 0;

	//setup rendering
	Vulkan::StartFrame(s_render_inst->backbuffer_pos);

	current_use_pool = s_render_inst->graphics_queue.GetCommandPool("test getting thing command pool");
	current_command_list = current_use_pool->StartCommandList("test getting thing command list");
}

void  Render::EndFrame()
{
	//render
	StartRenderingInfo start_rendering_info;
	start_rendering_info.viewport_width = s_render_inst->swapchain_width;
	start_rendering_info.viewport_height = s_render_inst->swapchain_height;
	start_rendering_info.initial_layout = RENDER_IMAGE_LAYOUT::UNDEFINED;
	start_rendering_info.final_layout = RENDER_IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL;
	start_rendering_info.load_color = false;
	start_rendering_info.store_color = true;
	start_rendering_info.clear_color_rgba = float4{ 1.f, 1.f, 0.f, 1.f };
	Vulkan::StartRendering(current_command_list.api_cmd_list, start_rendering_info, s_render_inst->backbuffer_pos);

	//present
	EndRenderingInfo end_rendering_info;
	end_rendering_info.initial_layout = RENDER_IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL;
	end_rendering_info.final_layout = RENDER_IMAGE_LAYOUT::PRESENT;
	Vulkan::EndRendering(current_command_list.api_cmd_list, end_rendering_info, s_render_inst->backbuffer_pos);
	current_use_pool->EndCommandList(current_command_list);

	s_render_inst->frames[s_render_inst->backbuffer_pos].fence_value = s_render_inst->graphics_queue.GetNextFenceValue();
	s_render_inst->graphics_queue.ExecutePresentCommands(&current_command_list, 1, nullptr, nullptr, 0, s_render_inst->backbuffer_pos);
	s_render_inst->graphics_queue.ReturnPool(current_use_pool);

	//swap images
	Vulkan::EndFrame(s_render_inst->backbuffer_pos);
	s_render_inst->backbuffer_pos = (s_render_inst->backbuffer_pos + 1) % s_render_inst->backbuffer_count;
}

MeshHandle Render::CreateMesh(const CreateMeshInfo& a_create_info)
{
	Mesh mesh;
	mesh.vertex_buffer = AllocateFromVertexBuffer(a_create_info.vertices.sizeInBytes());
	mesh.index_buffer = AllocateFromIndexBuffer(a_create_info.vertices.sizeInBytes());
	mesh.mesh_descriptor_allocation = Vulkan::AllocateDescriptor(vertex_descriptor_layout);

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