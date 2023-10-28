#include "Renderer.hpp"
#include "VulkanRenderer.hpp"

#include "Storage/Slotmap.h"
#include "Program.h"

#include "ShaderCompiler.h"

#include "BBIntrin.h"

using namespace BB;

struct RenderFence
{
	uint64_t next_fence_value;
	uint64_t last_complete_value;
	RFence fence;
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

/// <summary>
/// Handles one large upload buffer and handles it as if it's seperate buffers by handling chunks.
/// </summary>
class UploadBufferPool
{
public:
	UploadBufferPool(Allocator a_sys_allocator, const size_t a_size_per_pool, const uint32_t a_pool_count)
		:	m_upload_view_count(a_pool_count)
	{
		const size_t upload_buffer_size = a_size_per_pool * m_upload_view_count;
		BB_ASSERT(upload_buffer_size < UINT32_MAX, "we use uint32_t for offset and size but the uploadbuffer size is bigger then UINT32_MAX");

		BufferCreateInfo buffer_info;
		buffer_info.type = BUFFER_TYPE::UPLOAD;
		buffer_info.size = upload_buffer_size;
		buffer_info.host_writable = true;
		buffer_info.name = "upload_buffer_pool";
		m_upload_buffer = Vulkan::CreateBuffer(buffer_info);
		m_upload_mem_start = Vulkan::MapBufferMemory(m_upload_buffer);

		m_views = BBnewArr(a_sys_allocator, m_upload_view_count, UploadBufferView);
		for (size_t i = 0; i < m_upload_view_count; i++)
		{
			m_views[i].upload_buffer_handle = m_upload_buffer;
			m_views[i].size = static_cast<uint32_t>(a_size_per_pool);
			m_views[i].offset = static_cast<uint32_t>(i * a_size_per_pool);
			m_views[i].view_mem_start = Pointer::Add(m_upload_mem_start, m_views[i].offset);
			m_views[i].fence_value = 0;
		}

		for (uint32_t i = 0; i < m_upload_view_count - 1; i++)
		{
			m_views[i].next = &m_views[i + 1];
		}

		m_free_views = m_views;
		m_in_flight_views = nullptr;
		m_lock = OSCreateRWLock();
		m_in_flight_lock = OSCreateRWLock();

		m_fence = Vulkan::CreateFence(0, "upload buffer fence");
		m_last_completed_fence_value = 0;
		m_next_fence_value = 1;
	}
	~UploadBufferPool()
	{
		Vulkan::UnmapBufferMemory(m_upload_buffer);
		Vulkan::FreeBuffer(m_upload_buffer);
	}

	UploadBufferView* GetUploadView(const char* a_pool_name = "")
	{
		OSAcquireSRWLockWrite(&m_lock);
		UploadBufferView* view = BB_SLL_POP(m_free_views);
		//TODO, handle nullptr, maybe just wait or do the reset here?
		OSReleaseSRWLockWrite(&m_lock);
		return view;
	}

	void IncrementNextFenceValue()
	{
		//yes
		//maybe a lock is faster as it can be uncontested. Which may often be the case.
		BBInterlockedIncrement64(reinterpret_cast<volatile long long*>(&m_next_fence_value));
	}

	void GetFence(RFence* a_fence, uint64_t* a_next_fence_value)
	{
		*a_fence = m_fence;
		*a_next_fence_value = m_next_fence_value;
	}

	//return a upload view and give it a fence value that you get from UploadBufferPool::GetFence
	void ReturnUploadView(UploadBufferView* a_view, const uint64_t a_fence_value)
	{
		a_view->fence_value = a_fence_value;
		OSAcquireSRWLockWrite(&m_in_flight_lock);
		BB_SLL_PUSH(m_in_flight_views, a_view);
		OSReleaseSRWLockWrite(&m_in_flight_lock);
	}

	void CheckIfInFlightDone()
	{
		UploadBufferView* local_view_free = nullptr;

		//lock as a write can be attempetd in m_in_flight_pools.
		OSAcquireSRWLockWrite(&m_in_flight_lock);
		m_last_completed_fence_value = Vulkan::GetCurrentFenceValue(m_fence);

		//Thank you Descent Raytracer teammates great code that I can steal
		for (UploadBufferView** in_flight_views = &m_in_flight_views; *in_flight_views;)
		{
			UploadBufferView* view = *in_flight_views;

			if (view->fence_value <= m_last_completed_fence_value)
			{
				//Get next in-flight commandlist
				*in_flight_views = view->next;
				BB_SLL_PUSH(local_view_free, view);
			}
			else
			{
				in_flight_views = &view->next;
			}
		}

		if (local_view_free)
		{
			//jank, find better way to do this.
			UploadBufferView* local_view_end = local_view_free;
			while (local_view_end->next != nullptr)
			{
				local_view_end = local_view_end->next;
			}
			local_view_end->next = m_free_views;
			m_free_views = local_view_free;
		}
		OSReleaseSRWLockWrite(&m_in_flight_lock);
	}

private:
	RBuffer m_upload_buffer;			  //8
	void* m_upload_mem_start;			  //16

	const uint32_t m_upload_view_count;	  //24
	UploadBufferView* m_views;			  //32
	UploadBufferView* m_free_views;		  //40
	UploadBufferView* m_in_flight_views;  //48
	BBRWLock m_in_flight_lock;		      //56
	BBRWLock m_lock;					  //64

	//this fence should be send to all execute commandlists when they include a upload buffer from here.
	RFence m_fence;						  //72
	volatile uint64_t m_next_fence_value; //80
	uint64_t m_last_completed_fence_value;//88
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
	RCommandList StartCommandList(const char* a_name = nullptr)
	{
		BB_ASSERT(m_recording == false, "already recording a commandlist from this commandpool!");
		BB_ASSERT(m_list_current_free < m_list_count, "command pool out of lists!");
		RCommandList list{ m_lists[m_list_current_free++] };
		Vulkan::StartCommandList(list, a_name);
		m_recording = true;
		return list;
	}

	void EndCommandList(RCommandList a_list)
	{
		BB_ASSERT(m_recording == true, "trying to end a commandlist while the pool is not recording any list");
		BB_ASSERT(a_list == m_lists[m_list_current_free - 1], "commandlist that was submitted is not from this pool or was already closed!");
		Vulkan::EndCommandList(a_list);
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

	RenderQueue(Allocator a_system_allocator, const QUEUE_TYPE a_queue_type, const char* a_name, const uint32_t a_command_pool_count, const uint32_t a_command_lists_per_pool)
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
		m_in_flight_lock = OSCreateRWLock();
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
		OSAcquireSRWLockWrite(&m_in_flight_lock);
		a_pool->m_fence_value = m_fence.next_fence_value;
		BB_SLL_PUSH(m_in_flight_pools, a_pool);
		OSReleaseSRWLockWrite(&m_in_flight_lock);
	}

	void ExecutePresentCommands(RCommandList* a_lists, const uint32_t a_list_count, const RFence* const a_signal_fences, const uint64_t* const a_signal_values, const uint32_t a_signal_count, const RFence* const a_wait_fences, const uint64_t* const a_wait_values, const uint32_t a_wait_count, const uint32_t a_backbuffer_index)
	{
		BB_ASSERT(m_queue_type == QUEUE_TYPE::GRAPHICS, "calling a present commands on a non-graphics command queue is not valid");
		
		const uint32_t signal_fence_count = 1 + a_signal_count;
		RFence* signal_fences = BBstackAlloc(signal_fence_count, RFence);
		uint64_t* signal_values = BBstackAlloc(signal_fence_count, uint64_t);
		Memory::Copy(signal_fences, a_signal_fences, a_signal_count);
		signal_fences[a_signal_count] = m_fence.fence;
		Memory::Copy(signal_values, a_signal_values, a_signal_count);
		signal_values[a_signal_count] = m_fence.next_fence_value;

		ExecuteCommandsInfo execute_info;
		execute_info.lists = a_lists;
		execute_info.list_count = a_list_count;
		execute_info.signal_fences = signal_fences;
		execute_info.signal_values = signal_values;
		execute_info.signal_count = signal_fence_count;
		execute_info.wait_fences = a_wait_fences;
		execute_info.wait_values = a_wait_values;
		execute_info.wait_count = a_wait_count;

		OSAcquireSRWLockWrite(&m_lock);
		Vulkan::ExecutePresentCommandList(m_queue, execute_info, a_backbuffer_index);
		++m_fence.next_fence_value;
		OSReleaseSRWLockWrite(&m_lock);
	}

	void ExecutePresentCommands(RCommandList* a_lists, const RFence* const a_signal_fences, const uint64_t* const a_signal_values, const uint32_t a_signal_count, const RFence* const a_wait_fences, const uint64_t* const a_wait_values, const uint32_t a_wait_count, const uint32_t a_backbuffer_index)
	{
		BB_ASSERT(m_queue_type == QUEUE_TYPE::GRAPHICS, "calling a present commands on a non-graphics command queue is not valid");
		
		const uint32_t signal_fence_count = 1 + a_signal_count;
		RFence* signal_fences = BBstackAlloc(signal_fence_count, RFence);
		uint64_t* signal_values = BBstackAlloc(signal_fence_count, uint64_t);
		Memory::Copy(signal_fences, a_signal_fences, a_signal_count);
		signal_fences[a_signal_count] = m_fence.fence;
		Memory::Copy(signal_values, a_signal_values, a_signal_count);
		signal_values[a_signal_count] = m_fence.next_fence_value;

		ExecuteCommandsInfo execute_info;
		execute_info.lists = a_lists;
		execute_info.list_count = 1;
		execute_info.signal_fences = signal_fences;
		execute_info.signal_values = signal_values;
		execute_info.signal_count = signal_fence_count;
		execute_info.wait_fences = a_wait_fences;
		execute_info.wait_values = a_wait_values;
		execute_info.wait_count = a_wait_count;

		OSAcquireSRWLockWrite(&m_lock);
		Vulkan::ExecutePresentCommandList(m_queue, execute_info, a_backbuffer_index);
		++m_fence.next_fence_value;
		OSReleaseSRWLockWrite(&m_lock);
	}

	//void ExecutePresentCommands(CommandList* a_CommandLists, const uint32_t a_CommandListCount, const RenderFence* a_WaitFences, const RENDER_PIPELINE_STAGE* a_WaitStages, const uint32_t a_FenceCount);
	void WaitFenceValue(const uint64_t a_fence_value)
	{
		CommandPool* local_list_free = nullptr;

		//lock as a write can be attempetd in m_in_flight_pools.

		Vulkan::WaitFence(m_fence.fence, a_fence_value);

		OSAcquireSRWLockWrite(&m_in_flight_lock);
		//Thank you Descent Raytracer teammates great code that I can steal
		for (CommandPool** in_flight_command_polls = &m_in_flight_pools; *in_flight_command_polls;)
		{
			CommandPool* command_pool = *in_flight_command_polls;

			if (command_pool->m_fence_value <= a_fence_value)
			{
				command_pool->ResetPool();

				//Get next in-flight commandlist
				*in_flight_command_polls = command_pool->next;
				BB_SLL_PUSH(local_list_free, command_pool);
			}
			else
			{
				in_flight_command_polls = &command_pool->next;
			}
		}

		if (local_list_free)
		{
			//jank, find better way to do this.
			CommandPool* local_list_end = local_list_free;
			while (local_list_end->next != nullptr)
			{
				local_list_end = local_list_end->next;
			}
			local_list_end->next = m_free_pools;
			m_free_pools = local_list_free;
		}
		OSReleaseSRWLockWrite(&m_in_flight_lock);
	}

	void WaitIdle()
	{
		WaitFenceValue(m_fence.next_fence_value - 1);
	}

	RenderFence GetFence() const { return m_fence; }
	uint64_t GetNextFenceValue() const { return m_fence.next_fence_value; }
	uint64_t GetLastCompletedValue() const { return m_fence.last_complete_value; }

private:
	QUEUE_TYPE m_queue_type; //4
	BBRWLock m_lock; //12
	const uint32_t m_pool_count; //16
	CommandPool* m_pools; //24
	CommandPool* m_free_pools; //32 
	CommandPool* m_in_flight_pools = nullptr; //40
	BBRWLock m_in_flight_lock; //48

	RQueue m_queue;//56 
	RenderFence m_fence; //80
};

struct Mesh
{
	BufferView vertex_buffer;
	BufferView index_buffer;
};

struct DrawList
{
	MeshHandle* mesh;
	float4x4* matrix;
};

constexpr uint32_t UPLOAD_BUFFER_POOL_SIZE = mbSize * 8;
constexpr uint32_t UPLOAD_BUFFER_POOL_COUNT = 8;

struct RenderInterface_inst
{
	RenderInterface_inst(Allocator a_system_allocator)
		: graphics_queue(a_system_allocator, QUEUE_TYPE::GRAPHICS, "graphics queue", 8, 8),
		  upload_buffers(a_system_allocator, UPLOAD_BUFFER_POOL_SIZE, UPLOAD_BUFFER_POOL_COUNT)
	{}

	WindowHandle swapchain_window;
	uint32_t swapchain_width;
	uint32_t swapchain_height;
	bool debug;

	ShaderCompiler shader_compiler;

	struct VertexBuffer
	{
		RBuffer buffer;
		uint32_t size;
		uint32_t used;
		DescriptorAllocation descriptor_allocation;
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

		BufferView scene_buffer;
		BufferView transform_buffer;
		DescriptorAllocation desc_alloc;
		uint64_t fence_value;
	} *frames;

	SceneInfo scene_info;

	StaticSlotmap<Mesh> mesh_map{};

	uint32_t draw_list_count;
	uint32_t draw_list_max;
	DrawList draw_list_data;

	RenderQueue graphics_queue;

	UploadBufferPool upload_buffers;
};

static RenderInterface_inst* s_render_inst;

CommandPool* current_use_pool;
RCommandList current_command_list;

RPipeline test_pipeline;
ShaderObject vertex_object;
ShaderObject fragment_object;
RDescriptorLayout global_descriptor_layout;
RDescriptorLayout frame_descriptor_layout;
RPipelineLayout pipeline_layout;
RDepthBuffer depth_buffer;

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

void BB::InitializeRenderer(StackAllocator_t& a_stack_allocator, const RendererCreateInfo& a_render_create_info)
{
	Vulkan::InitializeVulkan(a_stack_allocator, a_render_create_info.app_name, a_render_create_info.engine_name, a_render_create_info.debug);
	s_render_inst = BBnew(a_stack_allocator, RenderInterface_inst)(a_stack_allocator);
	s_render_inst->backbuffer_count = 3;
	s_render_inst->backbuffer_pos = 0;
	Vulkan::CreateSwapchain(a_stack_allocator, a_render_create_info.window_handle, a_render_create_info.swapchain_width, a_render_create_info.swapchain_height, s_render_inst->backbuffer_count);
	s_render_inst->frames = BBnewArr(a_stack_allocator, s_render_inst->backbuffer_count, RenderInterface_inst::Frame);

	s_render_inst->swapchain_window = a_render_create_info.window_handle;
	s_render_inst->swapchain_width = a_render_create_info.swapchain_width;
	s_render_inst->swapchain_height = a_render_create_info.swapchain_height;
	s_render_inst->debug = a_render_create_info.debug;

	s_render_inst->draw_list_max = 128;
	s_render_inst->draw_list_count = 0;
	s_render_inst->draw_list_data.mesh = BBnewArr(a_stack_allocator, s_render_inst->draw_list_max, MeshHandle);
	s_render_inst->draw_list_data.matrix = BBnewArr(a_stack_allocator, s_render_inst->draw_list_max, float4x4);

	s_render_inst->mesh_map.Init(a_stack_allocator, 32);

	s_render_inst->shader_compiler = CreateShaderCompiler(a_stack_allocator);

	BBStackAllocatorScope(a_stack_allocator)
	{
		{
			//global descriptor set 0
			DescriptorBindingInfo descriptor_bindings[1];
			descriptor_bindings[0].binding = 0;
			descriptor_bindings[0].count = 1;
			descriptor_bindings[0].shader_stage = SHADER_STAGE::VERTEX;
			descriptor_bindings[0].type = DESCRIPTOR_TYPE::READONLY_BUFFER;
			global_descriptor_layout = Vulkan::CreateDescriptorLayout(a_stack_allocator, Slice(descriptor_bindings, _countof(descriptor_bindings)));
		}
		{
			//per-frame descriptor set 1
			DescriptorBindingInfo descriptor_bindings[2];
			descriptor_bindings[0].binding = 0;
			descriptor_bindings[0].count = 1;
			descriptor_bindings[0].shader_stage = SHADER_STAGE::ALL;
			descriptor_bindings[0].type = DESCRIPTOR_TYPE::READONLY_BUFFER;

			descriptor_bindings[1].binding = 1;
			descriptor_bindings[1].count = 1;
			descriptor_bindings[1].shader_stage = SHADER_STAGE::VERTEX;
			descriptor_bindings[1].type = DESCRIPTOR_TYPE::READONLY_BUFFER;
			frame_descriptor_layout = Vulkan::CreateDescriptorLayout(a_stack_allocator, Slice(descriptor_bindings, _countof(descriptor_bindings)));
		}

		RDescriptorLayout desc_layouts[] = { global_descriptor_layout, frame_descriptor_layout };
		PushConstantRange push_constant;
		push_constant.stages = SHADER_STAGE::ALL;
		push_constant.offset = 0;
		push_constant.size = sizeof(ShaderIndices);
		pipeline_layout = Vulkan::CreatePipelineLayout(desc_layouts, _countof(desc_layouts), &push_constant, 1);

		const ShaderCode vertex_shader = CompileShader(a_stack_allocator, s_render_inst->shader_compiler, "../resources/shaders/hlsl/Debug.hlsl", "VertexMain", SHADER_STAGE::VERTEX);
		const ShaderCode fragment_shader = CompileShader(a_stack_allocator, s_render_inst->shader_compiler, "../resources/shaders/hlsl/Debug.hlsl", "FragmentMain", SHADER_STAGE::FRAGMENT_PIXEL);

		Buffer shader_buffer = GetShaderCodeBuffer(vertex_shader);

		ShaderObjectCreateInfo shader_objects_info[2];
		shader_objects_info[0].stage = SHADER_STAGE::VERTEX;
		shader_objects_info[0].next_stages = SHADER_STAGE::FRAGMENT_PIXEL;
		shader_objects_info[0].shader_code_size = shader_buffer.size;
		shader_objects_info[0].shader_code = shader_buffer.data;
		shader_objects_info[0].shader_entry = "VertexMain";
		shader_objects_info[0].descriptor_layout_count = _countof(desc_layouts);
		shader_objects_info[0].descriptor_layouts = desc_layouts;
		shader_objects_info[0].push_constant_range_count = 1;
		shader_objects_info[0].push_constant_ranges = &push_constant;

		shader_buffer = GetShaderCodeBuffer(fragment_shader);
		shader_objects_info[1].stage = SHADER_STAGE::FRAGMENT_PIXEL;
		shader_objects_info[1].next_stages = SHADER_STAGE::NONE;
		shader_objects_info[1].shader_code_size = shader_buffer.size;
		shader_objects_info[1].shader_code = shader_buffer.data;
		shader_objects_info[1].shader_entry = "FragmentMain";
		shader_objects_info[1].descriptor_layout_count = _countof(desc_layouts);
		shader_objects_info[1].descriptor_layouts = desc_layouts;
		shader_objects_info[1].push_constant_range_count = 1;
		shader_objects_info[1].push_constant_ranges = &push_constant;

		ShaderObject shader_objects[2];
		Vulkan::CreateShaderObject(a_stack_allocator, Slice(shader_objects_info, _countof(shader_objects_info)), shader_objects);

		vertex_object = shader_objects[0];
		fragment_object = shader_objects[1];

		CreatePipelineInfo pipe_info;
		pipe_info.layout = pipeline_layout;
		shader_buffer = GetShaderCodeBuffer(vertex_shader);
		pipe_info.vertex.shader_code_size = shader_buffer.size;
		pipe_info.vertex.shader_code = shader_buffer.data;
		pipe_info.vertex.shader_entry = "VertexMain";

		shader_buffer = GetShaderCodeBuffer(fragment_shader);
		pipe_info.fragment.shader_code_size = shader_buffer.size;
		pipe_info.fragment.shader_code = shader_buffer.data;
		pipe_info.fragment.shader_entry = "FragmentMain";

		pipe_info.depth_format = DEPTH_FORMAT::D24_UNORM_S8_UINT;
		test_pipeline = Vulkan::CreatePipeline(a_stack_allocator, pipe_info);

		ReleaseShaderCode(vertex_shader);
		ReleaseShaderCode(fragment_shader);
	}

	RenderDepthCreateInfo depth_create_info;
	depth_create_info.name = "stamdard depth buffer";
	depth_create_info.width = a_render_create_info.swapchain_width;
	depth_create_info.height = a_render_create_info.swapchain_height;
	depth_create_info.depth = 1;
	depth_create_info.depth_format = DEPTH_FORMAT::D24_UNORM_S8_UINT;

	depth_buffer = Vulkan::CreateDepthBuffer(depth_create_info);

	{
		BufferCreateInfo vertex_buffer;
		vertex_buffer.name = "global vertex buffer";
		vertex_buffer.size = mbSize * 64;
		vertex_buffer.type = BUFFER_TYPE::STORAGE; //using byteaddressbuffer to get the vertices
		vertex_buffer.host_writable = true;

		s_render_inst->vertex_buffer.buffer = Vulkan::CreateBuffer(vertex_buffer);
		s_render_inst->vertex_buffer.size = static_cast<uint32_t>(vertex_buffer.size);
		s_render_inst->vertex_buffer.used = 0;
		s_render_inst->vertex_buffer.descriptor_allocation = Vulkan::AllocateDescriptor(global_descriptor_layout);

		BufferView view;
		view.buffer = s_render_inst->vertex_buffer.buffer;
		view.offset = 0;
		view.size = s_render_inst->vertex_buffer.size;

		WriteDescriptorData vertex_descriptor_write{};
		vertex_descriptor_write.binding = 0;
		vertex_descriptor_write.descriptor_index = 0;
		vertex_descriptor_write.type = DESCRIPTOR_TYPE::READONLY_BUFFER;
		vertex_descriptor_write.buffer_view = view;

		WriteDescriptorInfos vertex_descriptor_info;
		vertex_descriptor_info.allocation = s_render_inst->vertex_buffer.descriptor_allocation;
		vertex_descriptor_info.descriptor_layout = global_descriptor_layout;
		vertex_descriptor_info.data = Slice(&vertex_descriptor_write, 1);

		Vulkan::WriteDescriptors(vertex_descriptor_info);
	}
	{
		BufferCreateInfo index_buffer;
		index_buffer.name = "global index buffer";
		index_buffer.size = mbSize * 64;
		index_buffer.type = BUFFER_TYPE::INDEX;
		index_buffer.host_writable = true;

		s_render_inst->index_buffer.buffer = Vulkan::CreateBuffer(index_buffer);
		s_render_inst->index_buffer.size = static_cast<uint32_t>(index_buffer.size);
		s_render_inst->index_buffer.used = 0;
	}


	BBStackAllocatorScope(a_stack_allocator)
	{
		//per frame stuff
		BufferCreateInfo per_frame_buffer_info;
		per_frame_buffer_info.name = "per_frame_buffer";
		per_frame_buffer_info.size = mbSize * 4;
		per_frame_buffer_info.type = BUFFER_TYPE::STORAGE;
		for (uint32_t i = 0; i < s_render_inst->backbuffer_count; i++)
		{
			auto& pf = s_render_inst->frames[i];
			{
				pf.per_frame_buffer.buffer = Vulkan::CreateBuffer(per_frame_buffer_info);
				pf.per_frame_buffer.size = static_cast<uint32_t>(per_frame_buffer_info.size);
				pf.per_frame_buffer.used = 0;
			}
			{
				pf.scene_buffer.buffer = pf.per_frame_buffer.buffer;
				pf.scene_buffer.size = sizeof(SceneInfo);
				pf.scene_buffer.offset = pf.per_frame_buffer.used;

				pf.per_frame_buffer.used += static_cast<uint32_t>(pf.scene_buffer.size);
			}
			{
				pf.transform_buffer.buffer = pf.per_frame_buffer.buffer;
				pf.transform_buffer.size = s_render_inst->draw_list_max * sizeof(float4x4);
				pf.transform_buffer.offset = pf.per_frame_buffer.used;

				pf.per_frame_buffer.used += static_cast<uint32_t>(pf.transform_buffer.size);
			}

			//descriptors
			pf.desc_alloc = Vulkan::AllocateDescriptor(frame_descriptor_layout);

			WriteDescriptorData per_scene_buffer_desc[2]{};
			per_scene_buffer_desc[0].binding = 0;
			per_scene_buffer_desc[0].descriptor_index = 0;
			per_scene_buffer_desc[0].type = DESCRIPTOR_TYPE::READONLY_BUFFER;
			per_scene_buffer_desc[0].buffer_view = pf.scene_buffer;

			per_scene_buffer_desc[1].binding = 1;
			per_scene_buffer_desc[1].descriptor_index = 0;
			per_scene_buffer_desc[1].type = DESCRIPTOR_TYPE::READONLY_BUFFER;
			per_scene_buffer_desc[1].buffer_view = pf.transform_buffer;

			WriteDescriptorInfos frame_desc_write;
			frame_desc_write.allocation = pf.desc_alloc;
			frame_desc_write.descriptor_layout = frame_descriptor_layout;
			frame_desc_write.data = Slice(per_scene_buffer_desc, _countof(per_scene_buffer_desc));

			Vulkan::WriteDescriptors(frame_desc_write);
		}
	}
}

void BB::StartFrame()
{
	s_render_inst->graphics_queue.WaitFenceValue(s_render_inst->frames[s_render_inst->backbuffer_pos].fence_value);
	s_render_inst->upload_buffers.CheckIfInFlightDone();

	//clear the drawlist, maybe do this on a per-frame basis of we do GPU upload?
	s_render_inst->draw_list_count = 0;

	//setup rendering
	Vulkan::StartFrame(s_render_inst->backbuffer_pos);

	current_use_pool = s_render_inst->graphics_queue.GetCommandPool("test getting thing command pool");
	current_command_list = current_use_pool->StartCommandList("test getting thing command list");
}

void BB::EndFrame()
{
	const auto& cur_frame = s_render_inst->frames[s_render_inst->backbuffer_pos];

	//upload matrices
	//optimalization, upload previous frame matrices when using transfer buffer?
	UploadBufferView* pmatrix_upload_view = s_render_inst->upload_buffers.GetUploadView();
	uint32_t upload_used = 0;
	pmatrix_upload_view->MemoryCopy(&s_render_inst->scene_info, upload_used, sizeof(SceneInfo));
	upload_used += sizeof(SceneInfo);
	uint32_t matrix_offset = upload_used;
	pmatrix_upload_view->MemoryCopy(s_render_inst->draw_list_data.matrix, upload_used, sizeof(float4x4) * s_render_inst->draw_list_count);

	//upload to some GPU buffer here.
	RenderCopyBuffer matrix_buffer_copy;
	matrix_buffer_copy.src = pmatrix_upload_view->upload_buffer_handle;
	matrix_buffer_copy.dst = cur_frame.per_frame_buffer.buffer;
	RenderCopyBufferRegion buffer_regions[2]; // 0 = scene, 1 = matrix
	buffer_regions[0].src_offset = pmatrix_upload_view->offset;
	buffer_regions[0].dst_offset = cur_frame.scene_buffer.offset;
	buffer_regions[0].size = cur_frame.scene_buffer.size;

	buffer_regions[1].src_offset = pmatrix_upload_view->offset + matrix_offset;
	buffer_regions[1].dst_offset = cur_frame.transform_buffer.offset;
	buffer_regions[1].size = cur_frame.transform_buffer.size;
	matrix_buffer_copy.regions = Slice(buffer_regions, _countof(buffer_regions));
	Vulkan::CopyBuffer(current_command_list, matrix_buffer_copy);

	RFence upload_fence;
	uint64_t upload_fence_value;
	s_render_inst->upload_buffers.GetFence(&upload_fence, &upload_fence_value);
	s_render_inst->upload_buffers.ReturnUploadView(pmatrix_upload_view, upload_fence_value);
	s_render_inst->upload_buffers.IncrementNextFenceValue();

	//render
	StartRenderingInfo start_rendering_info;
	start_rendering_info.viewport_width = s_render_inst->swapchain_width;
	start_rendering_info.viewport_height = s_render_inst->swapchain_height;
	start_rendering_info.initial_layout = IMAGE_LAYOUT::UNDEFINED;
	start_rendering_info.final_layout = IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL;
	start_rendering_info.depth_buffer = depth_buffer;
	start_rendering_info.load_color = false;
	start_rendering_info.store_color = true;
	start_rendering_info.clear_color_rgba = float4{ 0.f, 0.f, 0.f, 1.f };
	Vulkan::StartRendering(current_command_list, start_rendering_info, s_render_inst->backbuffer_pos);

//#define _USE_G_PIPELINE

#ifdef _USE_G_PIPELINE
	Vulkan::BindPipeline(current_command_list, test_pipeline);
#endif //_USE_G_PIPELINE
	Vulkan::BindIndexBuffer(current_command_list, s_render_inst->index_buffer.buffer, 0);
	const uint32_t buffer_indices[] = { 0, 0 };
	const size_t buffer_offsets[]{ s_render_inst->vertex_buffer.descriptor_allocation.offset, cur_frame.desc_alloc.offset };
	Vulkan::SetDescriptorBufferOffset(current_command_list, pipeline_layout, 0, _countof(buffer_offsets), buffer_indices, buffer_offsets);

#ifndef _USE_G_PIPELINE
	const SHADER_STAGE shader_stages[]{ SHADER_STAGE::VERTEX, SHADER_STAGE::FRAGMENT_PIXEL };
	const ShaderObject shader_objects[]{ vertex_object, fragment_object };
	Vulkan::BindShaders(current_command_list, 2, shader_stages, shader_objects);
#endif //_USE_G_PIPELINE

	for (uint32_t i = 0; i < s_render_inst->draw_list_count; i++)
	{
		const Mesh& mesh = s_render_inst->mesh_map.find(s_render_inst->draw_list_data.mesh[i].handle);

		ShaderIndices shader_indices;
		shader_indices.transform_index = i;
		shader_indices.vertex_buffer_offset = static_cast<uint32_t>(mesh.vertex_buffer.offset);
		Vulkan::SetPushConstants(current_command_list, pipeline_layout, 0, sizeof(ShaderIndices), &shader_indices);

		Vulkan::DrawIndexed(current_command_list,
			static_cast<uint32_t>(mesh.index_buffer.size / sizeof(uint32_t)),
			1,
			static_cast<uint32_t>(mesh.index_buffer.offset / sizeof(uint32_t)),
			0,
			0);
	}

	//present
	EndRenderingInfo end_rendering_info;
	end_rendering_info.initial_layout = IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL;
	end_rendering_info.final_layout = IMAGE_LAYOUT::PRESENT;
	Vulkan::EndRendering(current_command_list, end_rendering_info, s_render_inst->backbuffer_pos);
	current_use_pool->EndCommandList(current_command_list);

	s_render_inst->frames[s_render_inst->backbuffer_pos].fence_value = s_render_inst->graphics_queue.GetNextFenceValue();
	s_render_inst->graphics_queue.ExecutePresentCommands(&current_command_list, &upload_fence, &upload_fence_value, 1, nullptr, nullptr, 0, s_render_inst->backbuffer_pos);
	s_render_inst->graphics_queue.ReturnPool(current_use_pool);

	//swap images
	Vulkan::EndFrame(s_render_inst->backbuffer_pos);
	s_render_inst->backbuffer_pos = (s_render_inst->backbuffer_pos + 1) % s_render_inst->backbuffer_count;
}

void BB::SetView(const float4x4& a_view)
{
	s_render_inst->scene_info.view = a_view;
}

void BB::SetProjection(const float4x4& a_proj)
{
	s_render_inst->scene_info.proj = a_proj;
}

MeshHandle BB::CreateMesh(const CreateMeshInfo& a_create_info)
{
	Mesh mesh;
	mesh.vertex_buffer = AllocateFromVertexBuffer(a_create_info.vertices.sizeInBytes());
	mesh.index_buffer = AllocateFromIndexBuffer(a_create_info.indices.sizeInBytes());

	void* vert = Vulkan::MapBufferMemory(mesh.vertex_buffer.buffer);
	memcpy(Pointer::Add(vert, mesh.vertex_buffer.offset), a_create_info.vertices.data(), a_create_info.vertices.sizeInBytes());
	Vulkan::UnmapBufferMemory(mesh.vertex_buffer.buffer);

	void* indices = Vulkan::MapBufferMemory(mesh.index_buffer.buffer);
	memcpy(Pointer::Add(indices, mesh.index_buffer.offset), a_create_info.indices.data(), a_create_info.indices.sizeInBytes());
	Vulkan::UnmapBufferMemory(mesh.index_buffer.buffer);

	//copy thing
	//RenderCopyBufferRegion copy_regions[2];
	//RenderCopyBuffer copy_buffer_infos[2];

	//copy_buffer_infos[0].dst = mesh.vertex_buffer.buffer;
	//copy_buffer_infos[0].src = ;
	//copy_regions[0].size = mesh.vertex_buffer.size;
	//copy_regions[0].dst_offset = mesh.vertex_buffer.offset;
	//copy_regions[0].src_offset;
	//copy_buffer_infos[0].regions = Slice(&copy_regions[0], 1);

	//copy_buffer_infos[1].dst = mesh.index_buffer.buffer;
	//copy_buffer_infos[1].src = ;
	//copy_regions[1].size = mesh.index_buffer.size;
	//copy_regions[1].dst_offset = mesh.index_buffer.offset;
	//copy_regions[1].src_offset;
	//copy_buffer_infos[1].regions = Slice(&copy_regions[1], 1);

	//Vulkan::CopyBuffers(, copy_buffer_infos, 2);

	return MeshHandle(s_render_inst->mesh_map.insert(mesh).handle);
}

void BB::FreeMesh(const MeshHandle a_mesh)
{
	s_render_inst->mesh_map.erase(a_mesh.handle);
}

void BB::DrawMesh(const MeshHandle a_mesh, const float4x4& a_transform)
{
	s_render_inst->draw_list_data.mesh[s_render_inst->draw_list_count] = a_mesh;
	s_render_inst->draw_list_data.matrix[s_render_inst->draw_list_count++] = a_transform;
}