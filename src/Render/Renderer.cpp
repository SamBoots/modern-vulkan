#include "Renderer.hpp"
#include "VulkanRenderer.hpp"

#include "Storage/Slotmap.h"
#include "Program.h"

#include "ShaderCompiler.h"

#include "Math.inl"

using namespace BB;

struct RenderFence
{
	uint64_t next_fence_value;
	uint64_t last_complete_value;
	RFence fence;
};

constexpr uint32_t MAX_TEXTURES = 1024;

class GPUTextureManager
{
public:
	GPUTextureManager();

	const RTexture UploadTexture(const UploadImageInfo& a_upload_info, const RCommandList a_list, UploadBufferView& a_upload_buffer);
	void FreeTexture(const RTexture a_texture);

private:
	struct TextureSlot
	{
		RImage image;
		RImageView view;
		uint32_t next_free;
	};

	uint32_t next_free;
	TextureSlot textures[MAX_TEXTURES];
	BBRWLock lock;

	//purple color
	RImage debug_texture;
};

namespace BB
{
	/// <summary>
	/// Handles one large upload buffer and handles it as if it's seperate buffers by handling chunks.
	/// </summary>
	class UploadBufferPool
	{
	public:
		UploadBufferPool(Allocator a_sys_allocator, const size_t a_size_per_pool, const uint32_t a_pool_count)
			: m_upload_view_count(a_pool_count)
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
			for (uint32_t i = 0; i < m_upload_view_count; i++)
			{
				m_views[i].upload_buffer_handle = m_upload_buffer;
				m_views[i].size = static_cast<uint32_t>(a_size_per_pool);
				m_views[i].offset = static_cast<uint32_t>(i * a_size_per_pool);
				m_views[i].used = 0;
				m_views[i].pool_index = i;
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

		UploadBufferView& GetUploadView(const size_t a_upload_size)
		{
			(void)a_upload_size; //not used yet.
			OSAcquireSRWLockWrite(&m_lock);
			UploadBufferView& view = *m_free_views.Pop();
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

		void GetFence(RFence& a_fence, uint64_t& a_next_fence_value)
		{
			a_fence = m_fence;
			a_next_fence_value = m_next_fence_value;
		}

		//return a upload view and set the fence value to the next fence value.
		void ReturnUploadViews(const BB::Slice<UploadBufferView> a_views, RFence& a_out_fence, uint64_t& a_out_next_fence_value)
		{
			OSAcquireSRWLockWrite(&m_in_flight_lock);
			for (size_t i = 0; i < a_views.size(); i++)
			{
				a_views[i].fence_value = m_next_fence_value;
				m_in_flight_views.Push(&m_views[a_views[i].pool_index]);
			}
			a_out_fence = m_fence;
			a_out_next_fence_value = m_next_fence_value;
			OSReleaseSRWLockWrite(&m_in_flight_lock);
		}

		void CheckIfInFlightDone()
		{
			LinkedList<UploadBufferView> local_view_free{};

			//lock as a write can be attempetd in m_in_flight_pools.
			OSAcquireSRWLockWrite(&m_in_flight_lock);
			m_last_completed_fence_value = Vulkan::GetCurrentFenceValue(m_fence);

			//Thank you Descent Raytracer teammates for the great code that I can steal
			for (UploadBufferView** in_flight_views = &m_in_flight_views.head; *in_flight_views;)
			{
				UploadBufferView* view = *in_flight_views;

				if (view->fence_value <= m_last_completed_fence_value)
				{
					//Get next in-flight commandlist
					*in_flight_views = view->next;
					local_view_free.Push(view);
				}
				else
				{
					in_flight_views = &view->next;
				}
			}

			if (local_view_free.HasEntry())
				m_free_views.MergeList(local_view_free);

			OSReleaseSRWLockWrite(&m_in_flight_lock);
		}

	private:
		RBuffer m_upload_buffer;			  //8
		void* m_upload_mem_start;			  //16

		UploadBufferView* m_views;			  //24
		LinkedList<UploadBufferView> m_free_views;		 //32
		LinkedList<UploadBufferView> m_in_flight_views;  //40
		BBRWLock m_in_flight_lock;		      //48
		BBRWLock m_lock;					  //56

		//this fence should be send to all execute commandlists when they include a upload buffer from here.
		RFence m_fence;						  //64
		volatile uint64_t m_next_fence_value; //72
		uint64_t m_last_completed_fence_value;//80

		const uint32_t m_upload_view_count;	  //84
	};
}

RCommandList CommandPool::StartCommandList(const char* a_name)
{
	BB_ASSERT(m_recording == false, "already recording a commandlist from this commandpool!");
	BB_ASSERT(m_list_current_free < m_list_count, "command pool out of lists!");
	RCommandList list{ m_lists[m_list_current_free++] };
	Vulkan::StartCommandList(list, a_name);
	m_recording = true;
	return list;
}

void CommandPool::EndCommandList(RCommandList a_list)
{
	BB_ASSERT(m_recording == true, "trying to end a commandlist while the pool is not recording any list");
	BB_ASSERT(a_list == m_lists[m_list_current_free - 1], "commandlist that was submitted is not from this pool or was already closed!");
	Vulkan::EndCommandList(a_list);
	m_recording = false;
}
void CommandPool::ResetPool()
{
	BB_ASSERT(m_recording == false, "trying to reset a pool while still recording");
	Vulkan::ResetCommandPool(m_api_cmd_pool);
	m_list_current_free = 0;
}

//THREAD SAFE: TRUE
class BB::RenderQueue
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
			m_pools[i].pool_index = i;
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

	CommandPool& GetCommandPool(const char* a_pool_name = "")
	{
		(void)a_pool_name; //not used yet.
		OSAcquireSRWLockWrite(&m_lock);
		CommandPool& pool = *m_free_pools.Pop();
		//TODO, handle nullptr, maybe just wait or do the reset here?
		OSReleaseSRWLockWrite(&m_lock);
		return pool;
	}

	void ReturnPool(CommandPool& a_pool)
	{
		OSAcquireSRWLockWrite(&m_in_flight_lock);
		a_pool.m_fence_value = m_fence.next_fence_value;
		m_in_flight_pools.Push(&m_pools[a_pool.pool_index]);
		OSReleaseSRWLockWrite(&m_in_flight_lock);
	}

	void ReturnPools(const Slice<CommandPool> a_pools)
	{
		OSAcquireSRWLockWrite(&m_in_flight_lock);
		for (size_t i = 0; i < a_pools.size(); i++)
		{
			a_pools[i].m_fence_value = m_fence.next_fence_value;
			m_in_flight_pools.Push(&m_pools[a_pools[i].pool_index]);
		}

		OSReleaseSRWLockWrite(&m_in_flight_lock);
	}

	void ExecuteCommands(RCommandList* a_lists, const uint32_t a_list_count, const RFence* const a_signal_fences, const uint64_t* const a_signal_values, const uint32_t a_signal_count, const RFence* const a_wait_fences, const uint64_t* const a_wait_values, const uint32_t a_wait_count)
	{
		BB_ASSERT(m_queue_type == QUEUE_TYPE::GRAPHICS, "calling a present commands on a non-graphics command queue is not valid");

		ExecuteCommandsInfo execute_info;
		execute_info.lists = a_lists;
		execute_info.list_count = a_list_count;
		execute_info.signal_fences = a_signal_fences;
		execute_info.signal_values = a_signal_values;
		execute_info.signal_count = a_signal_count;
		execute_info.wait_fences = a_wait_fences;
		execute_info.wait_values = a_wait_values;
		execute_info.wait_count = a_wait_count;

		OSAcquireSRWLockWrite(&m_lock);
		Vulkan::ExecuteCommandLists(m_queue, &execute_info, 1);
		++m_fence.next_fence_value;
		OSReleaseSRWLockWrite(&m_lock);
	}

	void ExecutePresentCommands(const RCommandList a_list, const RFence* const a_signal_fences, const uint64_t* const a_signal_values, const uint32_t a_signal_count, const RFence* const a_wait_fences, const uint64_t* const a_wait_values, const uint32_t a_wait_count, const uint32_t a_backbuffer_index)
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
		execute_info.lists = &a_list;
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

	//void ExecutePresentCommands(CommandList* a_CommandLists, const uint32_t a_CommandListCount, const RenderFence* a_WaitFences, const PIPELINE_STAGE* a_WaitStages, const uint32_t a_FenceCount);
	void WaitFenceValue(const uint64_t a_fence_value)
	{
		LinkedList<CommandPool> local_list_free;

		Vulkan::WaitFence(m_fence.fence, a_fence_value);

		//lock as a write can be attempetd in m_in_flight_pools.
		OSAcquireSRWLockWrite(&m_in_flight_lock);
		//Thank you Descent Raytracer teammates great code that I can steal
		for (CommandPool** in_flight_command_polls = &m_in_flight_pools.head; *in_flight_command_polls;)
		{
			CommandPool* command_pool = *in_flight_command_polls;

			if (command_pool->m_fence_value <= a_fence_value)
			{
				command_pool->ResetPool();

				//Get next in-flight commandlist
				*in_flight_command_polls = command_pool->next;
				local_list_free.Push(command_pool);
			}
			else
			{
				in_flight_command_polls = &command_pool->next;
			}
		}

		if (local_list_free.HasEntry())
			m_free_pools.MergeList(local_list_free);

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
	LinkedList<CommandPool> m_free_pools; //32 
	LinkedList<CommandPool> m_in_flight_pools; //40
	BBRWLock m_in_flight_lock; //48

	RQueue m_queue;//56 
	RenderFence m_fence; //80
};

struct Mesh
{
	BufferView vertex_buffer;
	BufferView index_buffer;
};

struct ShaderEffect
{
	const char* name;				//8
	ShaderObject shader_object;		//16
	RPipelineLayout pipeline_layout;//24
	SHADER_STAGE shader_stage;		//28
	SHADER_STAGE_FLAGS shader_stages_next; //32
};

struct Material
{
	struct Shader
	{
		RPipelineLayout pipeline_layout;
		
		ShaderObject shader_objects[UNIQUE_SHADER_STAGE_COUNT];
		SHADER_STAGE shader_stages[UNIQUE_SHADER_STAGE_COUNT];
		uint32_t shader_effect_count;
	} shader;


	RTexture base_color;
	RTexture normal_texture;
};

struct MeshDrawCall
{
	MeshHandle mesh;
	MaterialHandle material;
	uint32_t index_start;
	uint32_t index_count;
};

struct DrawList
{
	MeshDrawCall* mesh_draw_call;
	ShaderTransform* transform;
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

	GPUTextureManager texture_manager;

	RDescriptorLayout static_sampler_descriptor_set;
	RDescriptorLayout global_descriptor_set;
	DescriptorAllocation global_descriptor_allocation;
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

		BufferView scene_buffer;
		BufferView transform_buffer;
		DescriptorAllocation desc_alloc;
		uint64_t fence_value;
	} *frames;

	SceneInfo scene_info;

	StaticSlotmap<Mesh, MeshHandle> mesh_map{};
	StaticSlotmap<ShaderEffect, ShaderEffectHandle> shader_effect_map{};
	StaticSlotmap<Material, MaterialHandle> material_map{};

	uint32_t draw_list_count;
	uint32_t draw_list_max;
	DrawList draw_list_data;

	RenderQueue graphics_queue;

	UploadBufferPool upload_buffers;
};

static RenderInterface_inst* s_render_inst;

static CommandPool* current_use_pool;
static RCommandList current_command_list;


static RDescriptorLayout frame_descriptor_layout;

static RDepthBuffer depth_buffer;

static BufferView AllocateFromVertexBuffer(const size_t a_size_in_bytes)
{
	BufferView view;

	view.buffer = s_render_inst->vertex_buffer.buffer;
	view.size = a_size_in_bytes;
	view.offset = s_render_inst->vertex_buffer.used;

	s_render_inst->vertex_buffer.used += static_cast<uint32_t>(a_size_in_bytes);

	return view;
}

static BufferView AllocateFromIndexBuffer(const size_t a_size_in_bytes)
{
	BufferView view;

	view.buffer = s_render_inst->index_buffer.buffer;
	view.size = a_size_in_bytes;
	view.offset = s_render_inst->index_buffer.used;

	s_render_inst->index_buffer.used += static_cast<uint32_t>(a_size_in_bytes);

	return view;
}

GPUTextureManager::GPUTextureManager()
{
	const uint32_t purple = (209u << 24u) | (106u << 16u) | (255u << 8u) | (255u << 0u);
	(void)purple;

	lock = OSCreateRWLock();
	//texture 0 is always the debug texture.
	textures[0].image = debug_texture;
	textures[0].next_free = UINT32_MAX;

	next_free = 1;

	for (uint32_t i = 1; i < MAX_TEXTURES - 1; i++)
	{
		textures[i].image = debug_texture;
		textures[i].next_free = i + 1;
	}

	textures[MAX_TEXTURES - 1].image = debug_texture;
	textures[MAX_TEXTURES - 1].next_free = UINT32_MAX;
}

const RTexture GPUTextureManager::UploadTexture(const UploadImageInfo& a_upload_info, const RCommandList a_list, UploadBufferView& a_upload_buffer)
{
	OSAcquireSRWLockWrite(&lock);
	const RTexture texture_slot = RTexture(next_free);
	TextureSlot& slot = textures[texture_slot.handle];
	next_free = slot.next_free;
	OSReleaseSRWLockWrite(&lock);

	uint32_t byte_per_pixel;
	{
		ImageCreateInfo image_info;
		image_info.name = a_upload_info.name;
		image_info.width = a_upload_info.width;
		image_info.height = a_upload_info.height;
		image_info.depth = 1;
		image_info.array_layers = 1;
		image_info.mip_levels = 1;

		image_info.type = IMAGE_TYPE::TYPE_2D;
		image_info.tiling = IMAGE_TILING::OPTIMAL;

		switch (a_upload_info.bit_count)
		{
		case 32:
			image_info.format = IMAGE_FORMAT::RGBA8_SRGB;
			byte_per_pixel = 4;
			break;
		default:
			BB_ASSERT(false, "Unsupported bit_count for upload image");
			image_info.format = IMAGE_FORMAT::RGBA8_SRGB;
			byte_per_pixel = 4;
			break;
		}

		slot.image = Vulkan::CreateImage(image_info);

		ImageViewCreateInfo image_view_info;
		image_view_info.image = slot.image;
		image_view_info.name = a_upload_info.name;
		image_view_info.array_layers = 1;
		image_view_info.mip_levels = 1;
		image_view_info.type = image_info.type;
		image_view_info.format = image_info.format;
		slot.view = Vulkan::CreateViewImage(image_view_info);

		//pipeline barrier
		PipelineBarrierImageInfo image_write_transition;
		image_write_transition.src_mask = BARRIER_ACCESS_MASK::NONE;
		image_write_transition.dst_mask = BARRIER_ACCESS_MASK::TRANSFER_WRITE;
		image_write_transition.image = slot.image;
		image_write_transition.old_layout = IMAGE_LAYOUT::UNDEFINED;
		image_write_transition.new_layout = IMAGE_LAYOUT::TRANSFER_DST;
		image_write_transition.layer_count = 1;
		image_write_transition.level_count = 1;
		image_write_transition.base_array_layer = 0;
		image_write_transition.base_mip_level = 0;
		image_write_transition.src_stage = BARRIER_PIPELINE_STAGE::TOP_OF_PIPELINE;
		image_write_transition.dst_stage = BARRIER_PIPELINE_STAGE::TRANSFER;

		PipelineBarrierInfo pipeline_info{};
		pipeline_info.image_info_count = 1;
		pipeline_info.image_infos = &image_write_transition;
		Vulkan::PipelineBarriers(a_list, pipeline_info);
	}

	//now upload the image.
	uint32_t allocation_offset;
	a_upload_buffer.AllocateAndMemoryCopy(a_upload_info.pixels, byte_per_pixel * a_upload_info.width * a_upload_info.height, allocation_offset);


	RenderCopyBufferToImageInfo buffer_to_image;
	buffer_to_image.src_buffer = a_upload_buffer.GetBufferHandle();
	buffer_to_image.src_offset = allocation_offset;

	buffer_to_image.dst_image = slot.image;
	buffer_to_image.dst_image_info.size_x = a_upload_info.width;
	buffer_to_image.dst_image_info.size_y = a_upload_info.height;
	buffer_to_image.dst_image_info.size_z = 1;
	buffer_to_image.dst_image_info.offset_x = 0;
	buffer_to_image.dst_image_info.offset_y = 0;
	buffer_to_image.dst_image_info.offset_z = 0;
	buffer_to_image.dst_image_info.layout = IMAGE_LAYOUT::TRANSFER_DST;
	buffer_to_image.dst_image_info.mip_level = 0;
	buffer_to_image.dst_image_info.layer_count = 1;
	buffer_to_image.dst_image_info.base_array_layer = 0;

	Vulkan::CopyBufferImage(a_list, buffer_to_image);

	{
		PipelineBarrierImageInfo image_shader_transition;
		image_shader_transition.src_mask = BARRIER_ACCESS_MASK::TRANSFER_WRITE;
		image_shader_transition.dst_mask = BARRIER_ACCESS_MASK::SHADER_READ;
		image_shader_transition.image = slot.image;
		image_shader_transition.old_layout = IMAGE_LAYOUT::TRANSFER_DST;
		image_shader_transition.new_layout = IMAGE_LAYOUT::SHADER_READ_ONLY;
		image_shader_transition.layer_count = 1;
		image_shader_transition.level_count = 1;
		image_shader_transition.base_array_layer = 0;
		image_shader_transition.base_mip_level = 0;
		image_shader_transition.src_stage = BARRIER_PIPELINE_STAGE::TRANSFER;
		image_shader_transition.dst_stage = BARRIER_PIPELINE_STAGE::FRAGMENT_SHADER;

		PipelineBarrierInfo pipeline_info{};
		pipeline_info.image_info_count = 1;
		pipeline_info.image_infos = &image_shader_transition;
		Vulkan::PipelineBarriers(a_list, pipeline_info);
	}

	WriteDescriptorData write_desc{};
	write_desc.binding = 1;
	write_desc.descriptor_index = texture_slot.handle; //handle is also the descriptor index
	write_desc.type = DESCRIPTOR_TYPE::IMAGE;
	write_desc.image_view.view = slot.view;
	write_desc.image_view.layout = IMAGE_LAYOUT::SHADER_READ_ONLY;

	WriteDescriptorInfos image_write_infos;
	image_write_infos.allocation = s_render_inst->global_descriptor_allocation;
	image_write_infos.descriptor_layout = s_render_inst->global_descriptor_set;
	image_write_infos.data = Slice(&write_desc, 1);

	Vulkan::WriteDescriptors(image_write_infos);

	return texture_slot;
}

void GPUTextureManager::FreeTexture(const RTexture a_texture)
{
	TextureSlot& slot = textures[a_texture.handle];
	Vulkan::FreeImage(slot.image);
	Vulkan::FreeViewImage(slot.view);

	OSAcquireSRWLockWrite(&lock);
	slot.next_free = next_free;
	next_free = a_texture.handle;
	OSReleaseSRWLockWrite(&lock);
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
	s_render_inst->draw_list_data.mesh_draw_call = BBnewArr(a_stack_allocator, s_render_inst->draw_list_max, MeshDrawCall);
	s_render_inst->draw_list_data.transform = BBnewArr(a_stack_allocator, s_render_inst->draw_list_max, ShaderTransform);

	s_render_inst->mesh_map.Init(a_stack_allocator, 32);
	s_render_inst->shader_effect_map.Init(a_stack_allocator, 32);
	s_render_inst->material_map.Init(a_stack_allocator, 64);

	s_render_inst->shader_compiler = CreateShaderCompiler(a_stack_allocator);

	BBStackAllocatorScope(a_stack_allocator)
	{
		{	//static sampler descriptor set 0
			SamplerCreateInfo immutable_sampler{};
			immutable_sampler.name = "standard sampler";
			immutable_sampler.mode_u = SAMPLER_ADDRESS_MODE::REPEAT;
			immutable_sampler.mode_v = SAMPLER_ADDRESS_MODE::REPEAT;
			immutable_sampler.mode_w = SAMPLER_ADDRESS_MODE::REPEAT;
			immutable_sampler.filter = SAMPLER_FILTER::LINEAR;
			immutable_sampler.max_anistoropy = 1.0f;
			immutable_sampler.max_lod = 100.f;
			immutable_sampler.min_lod = -100.f;
			s_render_inst->static_sampler_descriptor_set = Vulkan::CreateDescriptorSamplerLayout(Slice(&immutable_sampler, 1));
		}
		{
			//global descriptor set 1
			DescriptorBindingInfo descriptor_bindings[2];
			descriptor_bindings[0].binding = 0;
			descriptor_bindings[0].count = 1;
			descriptor_bindings[0].shader_stage = SHADER_STAGE::VERTEX;
			descriptor_bindings[0].type = DESCRIPTOR_TYPE::READONLY_BUFFER;

			descriptor_bindings[1].binding = 1;
			descriptor_bindings[1].count = MAX_TEXTURES;
			descriptor_bindings[1].shader_stage = SHADER_STAGE::FRAGMENT_PIXEL;
			descriptor_bindings[1].type = DESCRIPTOR_TYPE::IMAGE;
			s_render_inst->global_descriptor_set = Vulkan::CreateDescriptorLayout(a_stack_allocator, Slice(descriptor_bindings, _countof(descriptor_bindings)));
			s_render_inst->global_descriptor_allocation = Vulkan::AllocateDescriptor(s_render_inst->global_descriptor_set);
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
		vertex_descriptor_info.allocation = s_render_inst->global_descriptor_allocation;
		vertex_descriptor_info.descriptor_layout = s_render_inst->global_descriptor_set;
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

	current_use_pool = &s_render_inst->graphics_queue.GetCommandPool("test getting thing command pool");
	current_command_list = current_use_pool->StartCommandList("test getting thing command list");
}

void BB::EndFrame()
{
	const auto& cur_frame = s_render_inst->frames[s_render_inst->backbuffer_pos];

	const uint32_t scene_upload_size = sizeof(SceneInfo);
	const uint32_t matrices_upload_size = sizeof(ShaderTransform) * s_render_inst->draw_list_count;

	//upload matrices
	//optimalization, upload previous frame matrices when using transfer buffer?
	UploadBufferView& matrix_upload_view = s_render_inst->upload_buffers.GetUploadView(static_cast<size_t>(scene_upload_size) + matrices_upload_size);

	uint32_t scene_offset = 0;
	matrix_upload_view.AllocateAndMemoryCopy(&s_render_inst->scene_info, scene_upload_size, scene_offset);
	uint32_t matrix_offset = 0;
	matrix_upload_view.AllocateAndMemoryCopy(s_render_inst->draw_list_data.transform, matrices_upload_size, matrix_offset);

	//upload to some GPU buffer here.
	RenderCopyBuffer matrix_buffer_copy;
	matrix_buffer_copy.src = matrix_upload_view.GetBufferHandle();
	matrix_buffer_copy.dst = cur_frame.per_frame_buffer.buffer;
	RenderCopyBufferRegion buffer_regions[2]; // 0 = scene, 1 = matrix
	buffer_regions[0].src_offset = scene_offset;
	buffer_regions[0].dst_offset = cur_frame.scene_buffer.offset;
	buffer_regions[0].size = cur_frame.scene_buffer.size;

	buffer_regions[1].src_offset = matrix_offset;
	buffer_regions[1].dst_offset = cur_frame.transform_buffer.offset;
	buffer_regions[1].size = s_render_inst->draw_list_count * sizeof(ShaderTransform);
	matrix_buffer_copy.regions = Slice(buffer_regions, _countof(buffer_regions));
	Vulkan::CopyBuffer(current_command_list, matrix_buffer_copy);

	RFence upload_fence;
	uint64_t upload_fence_value;
	s_render_inst->upload_buffers.ReturnUploadViews(Slice(&matrix_upload_view, 1), upload_fence, upload_fence_value);
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
	start_rendering_info.clear_color_rgba = float4{ 0.f, 0.5f, 0.f, 1.f };
	Vulkan::StartRendering(current_command_list, start_rendering_info, s_render_inst->backbuffer_pos);

	{
		//set the first data to get the first 3 descriptor sets.
		const MeshDrawCall& mesh_draw_call = s_render_inst->draw_list_data.mesh_draw_call[0];
		const Material& material = s_render_inst->material_map.find(mesh_draw_call.material);

		//set 0
		Vulkan::SetDescriptorImmutableSamplers(current_command_list, material.shader.pipeline_layout);
		const uint32_t buffer_indices[] = { 0, 0 };
		const size_t buffer_offsets[]{ s_render_inst->global_descriptor_allocation.offset, cur_frame.desc_alloc.offset };
		//set 1-2
		Vulkan::SetDescriptorBufferOffset(current_command_list,
			material.shader.pipeline_layout,
			SPACE_GLOBAL,
			_countof(buffer_offsets),
			buffer_indices,
			buffer_offsets);
	}

	Vulkan::BindIndexBuffer(current_command_list, s_render_inst->index_buffer.buffer, 0);

	for (uint32_t i = 0; i < s_render_inst->draw_list_count; i++)
	{
		const MeshDrawCall& mesh_draw_call = s_render_inst->draw_list_data.mesh_draw_call[i];
		const Material& material = s_render_inst->material_map.find(mesh_draw_call.material);
		const Mesh& mesh = s_render_inst->mesh_map.find(mesh_draw_call.mesh);

		Vulkan::BindShaders(current_command_list, 
			material.shader.shader_effect_count, 
			material.shader.shader_stages, 
			material.shader.shader_objects);

		ShaderIndices shader_indices;
		shader_indices.transform_index = i;
		shader_indices.vertex_buffer_offset = static_cast<uint32_t>(mesh.vertex_buffer.offset);
		shader_indices.albedo_texture = material.base_color.handle;

		Vulkan::SetPushConstants(current_command_list, material.shader.pipeline_layout, 0, sizeof(ShaderIndices), &shader_indices);

		Vulkan::DrawIndexed(current_command_list,
			static_cast<uint32_t>(mesh.index_buffer.size / sizeof(uint32_t)) + mesh_draw_call.index_count,
			1,
			static_cast<uint32_t>(mesh.index_buffer.offset / sizeof(uint32_t)) + mesh_draw_call.index_start,
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
	s_render_inst->graphics_queue.ExecutePresentCommands(current_command_list, &upload_fence, &upload_fence_value, 1, nullptr, nullptr, 0, s_render_inst->backbuffer_pos);
	s_render_inst->graphics_queue.ReturnPool(*current_use_pool);

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

UploadBufferView& BB::GetUploadView(const size_t a_upload_size)
{
	return s_render_inst->upload_buffers.GetUploadView(a_upload_size);
}

CommandPool& BB::GetGraphicsCommandPool()
{
	return s_render_inst->graphics_queue.GetCommandPool();
}

//MOCK, todo, uses graphics queue
CommandPool& BB::GetTransferCommandPool()
{
	return GetGraphicsCommandPool();
}

bool BB::ExecuteGraphicCommands(const BB::Slice<CommandPool> a_cmd_pools, const BB::Slice<UploadBufferView> a_upload_views)
{
	uint32_t list_count = 0;
	for (size_t i = 0; i < a_cmd_pools.size(); i++)
		list_count += a_cmd_pools[i].GetListsRecorded();

	RCommandList* lists = BBstackAlloc(list_count, RCommandList);
	list_count = 0;
	for (size_t i = 0; i < a_cmd_pools.size(); i++)
	{
		Memory::Copy(&lists[list_count],
			a_cmd_pools[i].GetLists(),
			a_cmd_pools[i].GetListsRecorded());
		list_count += a_cmd_pools[i].GetListsRecorded();
	}

	RFence upload_fence;
	uint64_t upload_fence_value;
	s_render_inst->upload_buffers.ReturnUploadViews(a_upload_views, upload_fence, upload_fence_value);
	s_render_inst->upload_buffers.IncrementNextFenceValue();

	s_render_inst->graphics_queue.ExecuteCommands(lists, list_count, &upload_fence, &upload_fence_value, 1, nullptr, nullptr, 0);
	s_render_inst->graphics_queue.ReturnPools(a_cmd_pools);
	return true;
}

//MOCK, todo, uses graphics queue
bool BB::ExecuteTransferCommands(const BB::Slice<CommandPool> a_cmd_pools, const BB::Slice<UploadBufferView> a_upload_views)
{
	return ExecuteGraphicCommands(a_cmd_pools, a_upload_views);
}

const MeshHandle BB::CreateMesh(const CreateMeshInfo& a_create_info)
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
	s_render_inst->mesh_map.erase(a_mesh);
}

bool BB::CreateShaderEffect(Allocator a_temp_allocator, const Slice<CreateShaderEffectInfo> a_create_infos, ShaderEffectHandle* a_handles)
{
	//our default layouts
	RDescriptorLayout desc_layouts[] = {
			s_render_inst->static_sampler_descriptor_set,
			s_render_inst->global_descriptor_set,
			frame_descriptor_layout };

	//all of them use this push constant for the shader indices.
	PushConstantRange push_constant;
	push_constant.stages = SHADER_STAGE::ALL;
	push_constant.offset = 0;
	push_constant.size = sizeof(ShaderIndices);

	ShaderEffect* shader_effects = BBnewArr(a_temp_allocator, a_create_infos.size(), ShaderEffect);
	ShaderCode* shader_codes = BBnewArr(a_temp_allocator, a_create_infos.size(), ShaderCode);
	ShaderObjectCreateInfo* shader_object_infos = BBnewArr(a_temp_allocator, a_create_infos.size(), ShaderObjectCreateInfo);

	for (size_t i = 0; i < a_create_infos.size(); i++)
	{
		shader_effects[i].pipeline_layout = Vulkan::CreatePipelineLayout(desc_layouts, _countof(desc_layouts), &push_constant, 1);

		shader_codes[i] = CompileShader(
			a_temp_allocator,
			s_render_inst->shader_compiler,
			a_create_infos[i].shader_path,
			a_create_infos[i].shader_entry,
			a_create_infos[i].stage);
		Buffer shader_buffer = GetShaderCodeBuffer(shader_codes[i]);

		shader_object_infos[i].stage = a_create_infos[i].stage;
		shader_object_infos[i].next_stages = a_create_infos[i].next_stages;
		shader_object_infos[i].shader_code_size = shader_buffer.size;
		shader_object_infos[i].shader_code = shader_buffer.data;
		shader_object_infos[i].shader_entry = a_create_infos[i].shader_entry;

		shader_object_infos[i].descriptor_layout_count = _countof(desc_layouts);
		shader_object_infos[i].descriptor_layouts = desc_layouts;
		shader_object_infos[i].push_constant_range_count = 1;
		shader_object_infos[i].push_constant_ranges = &push_constant;
	}

	ShaderObject* shader_objects = BBnewArr(a_temp_allocator, a_create_infos.size(), ShaderObject);
	Vulkan::CreateShaderObject(a_temp_allocator, Slice(shader_object_infos, a_create_infos.size()), shader_objects);

	for (size_t i = 0; i < a_create_infos.size(); i++)
	{
		shader_effects[i].name = shader_effects[i].name;
		shader_effects[i].shader_object = shader_objects[i];
		shader_effects[i].shader_stage = a_create_infos[i].stage;
		shader_effects[i].shader_stages_next = a_create_infos[i].next_stages;
		a_handles[i] = s_render_inst->shader_effect_map.insert(shader_effects[i]);
		ReleaseShaderCode(shader_codes[i]);
	}
	return true;
}

void BB::FreeShaderEffect(const ShaderEffectHandle a_shader_effect)
{
	ShaderEffect shader_effect = s_render_inst->shader_effect_map.find(a_shader_effect);
	Vulkan::DestroyShaderObject(shader_effect.shader_object);
	Vulkan::FreePipelineLayout(shader_effect.pipeline_layout);
	s_render_inst->shader_effect_map.erase(a_shader_effect);
}

const MaterialHandle BB::CreateMaterial(const CreateMaterialInfo& a_create_info)
{
	BB_ASSERT(UNIQUE_SHADER_STAGE_COUNT >= a_create_info.shader_effects.size(), "too many shader stages!");
	BB_ASSERT(UNIQUE_SHADER_STAGE_COUNT != 0, "no shader effects in material!");

	Material mat;
	//get the first pipeline layout, compare it with all of the ones in the other shaders.
	RPipelineLayout chosen_layout = s_render_inst->shader_effect_map.find(a_create_info.shader_effects[0]).pipeline_layout;
	
	SHADER_STAGE_FLAGS valid_next_stages = static_cast<uint32_t>(SHADER_STAGE::ALL);
	for (size_t i = 0; i < a_create_info.shader_effects.size(); i++)
	{
		//maybe check if we have duplicate shader stages;
		const ShaderEffect& effect = s_render_inst->shader_effect_map.find(a_create_info.shader_effects[i]);
		BB_ASSERT(chosen_layout == effect.pipeline_layout, "pipeline layouts are not the same for the shader effects");
		
		if (i < a_create_info.shader_effects.size())
		{
			BB_ASSERT((valid_next_stages & static_cast<SHADER_STAGE_FLAGS>(effect.shader_stage)) == static_cast<SHADER_STAGE_FLAGS>(effect.shader_stage), 
				"shader stage is not valid for the next shader stage of the previous shader object");
			valid_next_stages = effect.shader_stages_next;
		}

		mat.shader.shader_objects[i] = effect.shader_object;
		mat.shader.shader_stages[i] = effect.shader_stage;
	}
	mat.shader.shader_effect_count = static_cast<uint32_t>(a_create_info.shader_effects.size());
	mat.shader.pipeline_layout = chosen_layout;
	mat.base_color = a_create_info.base_color;
	mat.normal_texture = a_create_info.normal_texture;

	return MaterialHandle(s_render_inst->material_map.insert(mat).handle);
}

void BB::FreeMaterial(const MaterialHandle a_material)
{
	//maybe go and check the refcount of the textures to possibly free them.
	s_render_inst->material_map.erase(a_material);
}

//maybe not handle a_upload_view_offset
const RTexture BB::UploadTexture(const UploadImageInfo& a_upload_info, const RCommandList a_list, UploadBufferView& a_upload_view)
{
	return s_render_inst->texture_manager.UploadTexture(a_upload_info, a_list, a_upload_view);
}

void BB::FreeTexture(const RTexture a_texture)
{
	return s_render_inst->texture_manager.FreeTexture(a_texture);
}

void BB::DrawMesh(const MeshHandle a_mesh, const float4x4& a_transform, const uint32_t a_index_start, const uint32_t a_index_count, const MaterialHandle a_material)
{
	s_render_inst->draw_list_data.mesh_draw_call[s_render_inst->draw_list_count].mesh = a_mesh;
	s_render_inst->draw_list_data.mesh_draw_call[s_render_inst->draw_list_count].material = a_material;
	s_render_inst->draw_list_data.mesh_draw_call[s_render_inst->draw_list_count].index_start = a_index_start;
	s_render_inst->draw_list_data.mesh_draw_call[s_render_inst->draw_list_count].index_count = a_index_count;
	s_render_inst->draw_list_data.transform[s_render_inst->draw_list_count].transform = a_transform;
	s_render_inst->draw_list_data.transform[s_render_inst->draw_list_count++].inverse = Float4x4Inverse(a_transform);
}
