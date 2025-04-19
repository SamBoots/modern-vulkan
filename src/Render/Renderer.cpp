#include "Renderer.hpp"
#include "VulkanRenderer.hpp"

#include "Storage/Slotmap.h"
#include "Storage/Queue.hpp"
#include "Program.h"

#include "ShaderCompiler.h"

#include "Math.inl"

#include "imgui.h"
#include "implot.h"

#include "BBThreadScheduler.hpp"

#include "GPUBuffers.hpp"

using namespace BB;

constexpr float3 SKYBOX_VERTICES[] = {
	{-1.0f,-1.0f,-1.0f},  // -X side
	{-1.0f,-1.0f, 1.0f},
	{-1.0f, 1.0f, 1.0f},
	{-1.0f, 1.0f, 1.0f},
	{-1.0f, 1.0f,-1.0f},
	{-1.0f,-1.0f,-1.0f},

	{-1.0f,-1.0f,-1.0f},  // -Z side
	{ 1.0f, 1.0f,-1.0f},
	{ 1.0f,-1.0f,-1.0f},
	{-1.0f,-1.0f,-1.0f},
	{-1.0f, 1.0f,-1.0f},
	{ 1.0f, 1.0f,-1.0f},

	{-1.0f,-1.0f,-1.0f},  // -Y side
	{ 1.0f,-1.0f,-1.0f},
	{ 1.0f,-1.0f, 1.0f},
	{-1.0f,-1.0f,-1.0f},
	{ 1.0f,-1.0f, 1.0f},
	{-1.0f,-1.0f, 1.0f},

	{-1.0f, 1.0f,-1.0f},  // +Y side
	{-1.0f, 1.0f, 1.0f},
	{ 1.0f, 1.0f, 1.0f},
	{-1.0f, 1.0f,-1.0f},
	{ 1.0f, 1.0f, 1.0f},
	{ 1.0f, 1.0f,-1.0f},

	{ 1.0f, 1.0f,-1.0f},  // +X side
	{ 1.0f, 1.0f, 1.0f},
	{ 1.0f,-1.0f, 1.0f},
	{ 1.0f,-1.0f, 1.0f},
	{ 1.0f,-1.0f,-1.0f},
	{ 1.0f, 1.0f,-1.0f},

	{-1.0f, 1.0f, 1.0f},  // +Z side
	{-1.0f,-1.0f, 1.0f},
	{ 1.0f, 1.0f, 1.0f},
	{-1.0f,-1.0f, 1.0f},
	{ 1.0f,-1.0f, 1.0f},
	{ 1.0f, 1.0f, 1.0f},
};

struct RenderFence
{
	uint64_t next_fence_value;
	uint64_t last_complete_value;
	RFence fence;
};

struct TextureInfo
{
	RImage image;				// 8
	Slice<RImageView> views;	// 24
	uint32_t descriptor_index;	// 28

	uint32_t width;				// 32
	uint32_t height;			// 36
	uint32_t array_layers;		// 40
	uint32_t mip_levels;		// 44
	IMAGE_FORMAT format;		// 48
	IMAGE_LAYOUT current_layout;// 52

	// debug extra, not required for anything
	IMAGE_USAGE usage;			// 56
};

constexpr uint32_t MAX_TEXTURES = 1024;

class GPUTextureManager
{
public:
	void Init(MemoryArena& a_arena)
	{
		m_views = ArenaAllocArr(a_arena, RImageView, MAX_TEXTURES);
		m_next_free = 0;
		m_lock = OSCreateRWLock();

		// setup the freelist before making the debug texture
		for (uint32_t i = m_next_free; i < MAX_TEXTURES - 1; i++)
		{
			m_views[i].index = i + 1;
		}
		m_views[MAX_TEXTURES - 1].index = UINT32_MAX;
	}

	void SetAllTextures(const RDescriptorIndex a_descriptor_index, const RDescriptorLayout a_global_layout, const DescriptorAllocation& a_allocation) const;

	const RDescriptorIndex AllocAndWriteImageView(const ImageViewCreateInfo& a_info, const RDescriptorLayout a_global_layout, const DescriptorAllocation& a_allocation)
	{
		OSAcquireSRWLockWrite(&m_lock);
		const uint32_t descriptor_index = m_next_free;
		m_next_free = m_views[descriptor_index].index;
		OSReleaseSRWLockWrite(&m_lock);

		m_views[descriptor_index] =  Vulkan::CreateImageView(a_info);

		DescriptorWriteImageInfo write_info;
		write_info.binding = GLOBAL_BINDLESS_TEXTURES_BINDING;
		write_info.descriptor_index = descriptor_index;
		write_info.view = m_views[descriptor_index];
		write_info.layout = IMAGE_LAYOUT::RO_FRAGMENT;
		write_info.allocation = a_allocation;
		write_info.descriptor_layout = a_global_layout;

		DescriptorWriteImage(write_info);

		return RDescriptorIndex(descriptor_index);
	}

	void FreeImageView(const RDescriptorIndex a_descriptor_index, const RDescriptorLayout a_global_layout, const DescriptorAllocation& a_allocation);

	const RImageView GetImageView(const RDescriptorIndex a_index) const
	{
		return m_views[a_index.handle];
	}

	void DisplayTextureListImgui()
	{
		if (ImGui::CollapsingHeader("Texture Manager"))
		{
			ImGui::Indent();
			if (ImGui::CollapsingHeader("next free image slot"))
			{
				ImGui::Indent();
				ImGui::Text("RImageView index: %u", m_next_free);
				const ImVec2 image_size = { 160, 160 };

				ImGui::Image(m_next_free, image_size);
				ImGui::Unindent();
			}

			for (uint32_t i = 0; i < MAX_TEXTURES; i++)
			{
				const size_t id = i;
				if (ImGui::TreeNodeEx(reinterpret_cast<void*>(id), 0, "Texture Slot: %u", i))
				{
					ImGui::Indent();

					const ImVec2 image_size = { 160, 160 };
					ImGui::Image(i, image_size);

					ImGui::Unindent();
					ImGui::TreePop();
				}
			}
			ImGui::Unindent();
		}
	}

private:
	uint32_t m_next_free;
	RImageView* m_views;
	BBRWLock m_lock;
};

RCommandList CommandPool::StartCommandList(const char* a_name)
{
	BB_ASSERT(m_recording == false, "already recording a commandlist from this commandpool");
	BB_ASSERT(m_list_current_free < m_list_count, "command pool out of lists");
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
class RenderQueue
{
private:
	BBRWLock m_lock; //8
	QUEUE_TYPE m_queue_type; //12
	const uint32_t m_pool_count; //16
	CommandPool* m_pools; //24
	LinkedList<CommandPool> m_free_pools; //32 
	LinkedList<CommandPool> m_in_flight_pools; //40
	BBRWLock m_in_flight_lock; //48

	RQueue m_queue;//56 
	RenderFence m_fence; //80

public:
	RenderQueue(MemoryArena& a_arena, const QUEUE_TYPE a_queue_type, const char* a_name, const uint32_t a_command_pool_count, const uint32_t a_command_lists_per_pool)
		:	m_pool_count(a_command_pool_count)
	{
		m_queue_type = a_queue_type;
		m_queue = Vulkan::GetQueue(a_queue_type, a_name);
		m_fence.fence = Vulkan::CreateFence(0, "make_it_queue_name_sam!!!");
		m_fence.last_complete_value = 0;
		m_fence.next_fence_value = 1;

		m_pools = ArenaAllocArr(a_arena, CommandPool, a_command_pool_count);
		for (uint32_t i = 0; i < a_command_pool_count; i++)
		{
			m_pools[i].pool_index = i;
			m_pools[i].m_recording = false;
			m_pools[i].m_fence_value = 0;
			m_pools[i].m_list_count = a_command_lists_per_pool;
			m_pools[i].m_list_current_free = 0;
			m_pools[i].m_lists = ArenaAllocArr(a_arena, RCommandList, m_pools[i].m_list_count);
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

		if (m_free_pools.head == nullptr)
		{
			OSReleaseSRWLockWrite(&m_lock);
			WaitIdle();	// TODO: wait idle optimal? No
			OSAcquireSRWLockWrite(&m_lock);
		}
		CommandPool& pool = *m_free_pools.Pop();
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
			m_pools[a_pools[i].pool_index].m_fence_value = m_fence.next_fence_value;
			m_in_flight_pools.Push(&m_pools[a_pools[i].pool_index]);
		}

		OSReleaseSRWLockWrite(&m_in_flight_lock);
	}

	bool ExecuteCommands(const RCommandList* const a_lists, const uint32_t a_list_count, const RFence* const a_signal_fences, const uint64_t* const a_signal_values, const uint32_t a_signal_count, const RFence* const a_wait_fences, const uint64_t* const a_wait_values, const uint32_t a_wait_count, uint64_t& a_out_fence_value)
	{
		OSAcquireSRWLockWrite(&m_lock);
		//better way to do this?
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


		Vulkan::ExecuteCommandLists(m_queue, &execute_info, 1);
		a_out_fence_value = m_fence.next_fence_value++;
		OSReleaseSRWLockWrite(&m_lock);
		return true;
	}

	PRESENT_IMAGE_RESULT ExecutePresentCommands(RCommandList* const a_lists, const uint32_t a_list_count, const RFence* const a_signal_fences, const uint64_t* const a_signal_values, const uint32_t a_signal_count, const RFence* const a_wait_fences, const uint64_t* const a_wait_values, const uint32_t a_wait_count, const uint32_t a_backbuffer_index, uint64_t& a_out_fence_value)
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
		const PRESENT_IMAGE_RESULT result = Vulkan::ExecutePresentCommandList(m_queue, execute_info, a_backbuffer_index);
		a_out_fence_value = m_fence.next_fence_value++;
		OSReleaseSRWLockWrite(&m_lock);
		return result;
	}

	//void ExecutePresentCommands(CommandList* a_CommandLists, const uint32_t a_CommandListCount, const RenderFence* a_WaitFences, const PIPELINE_STAGE* a_WaitStages, const uint32_t a_FenceCount);
	void WaitFenceValue(const GPUFenceValue a_fence_value)
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
};

struct ShaderEffect
{
	const char* name;				//8
	ShaderObject shader_object;		//16
	RPipelineLayout pipeline_layout;//24
	SHADER_STAGE shader_stage;		//28
	SHADER_STAGE_FLAGS shader_stages_next; //32

#ifdef _ENABLE_REBUILD_SHADERS
	const char* shader_entry;
	ShaderObjectCreateInfo create_info;
#endif // _ENABLE_REBUILD_SHADERS
};

constexpr uint32_t BACK_BUFFER_MAX = 3;

struct RenderInterface_inst
{
	RenderInterface_inst(MemoryArena& a_arena)
		: graphics_queue(a_arena, QUEUE_TYPE::GRAPHICS, "graphics queue", 32, 32),
		  transfer_queue(a_arena, QUEUE_TYPE::TRANSFER, "transfer queue", 8, 8),
		  compute_queue(a_arena, QUEUE_TYPE::COMPUTE, "compute queue", 8, 8)
	{}

	struct Status
	{
		uint32_t frame_index;
		bool frame_started;
		bool frame_ended;
	} status;

	struct Frame
	{
		uint64_t graphics_queue_fence_value;
	};
	uint32_t frame_count;
	Frame* frames;
	bool debug;

	ShaderCompiler shader_compiler;

	RenderQueue graphics_queue;
	RenderQueue transfer_queue;
	RenderQueue compute_queue;

	GPUTextureManager texture_manager;

	GPUBufferView cubemap_position;
	RImage debug_texture;
	RDescriptorIndex debug_descriptor_index;

	RDescriptorLayout static_sampler_descriptor_set;
	RDescriptorLayout global_descriptor_set;
	DescriptorAllocation global_descriptor_allocation;

	struct GlobalBuffer
	{
		GlobalRenderData data;
		GPUBuffer buffer;
		void* mapped;
	} global_buffer;

	struct VertexBuffer
	{
		GPUBuffer buffer;
        GPUAddress address;
		uint64_t size;
		std::atomic<uint64_t> used;
	} vertex_buffer;
	struct CPUVertexBuffer
	{
		GPUBuffer buffer;
        GPUAddress address;
		uint64_t size;
		std::atomic<uint64_t> used;
		void* start_mapped;
	} cpu_vertex_buffer;

	struct IndexBuffer
	{
		GPUBuffer buffer;
        GPUAddress address;
		uint64_t size;
		std::atomic<uint64_t> used;
	} index_buffer;
	struct CPUIndexBuffer
	{
		GPUBuffer buffer;
        GPUAddress address;
		uint64_t size;
		std::atomic<uint64_t> used;
		void* start_mapped;
	} cpu_index_buffer;

	StaticSlotmap<ShaderEffect, ShaderEffectHandle> shader_effects{};
};

static RenderInterface_inst* s_render_inst;

void GPUTextureManager::SetAllTextures(const RDescriptorIndex a_descriptor_index, const RDescriptorLayout a_global_layout, const DescriptorAllocation& a_allocation) const
{
	DescriptorWriteImageInfo image_write;
	image_write.descriptor_layout = a_global_layout;
	image_write.allocation = a_allocation;
	image_write.view = GetImageView(a_descriptor_index);
	image_write.layout = IMAGE_LAYOUT::RO_FRAGMENT;
	image_write.binding = GLOBAL_BINDLESS_TEXTURES_BINDING;
	for (uint32_t i = 0; i < MAX_TEXTURES; i++)
	{
		image_write.descriptor_index = i;
		DescriptorWriteImage(image_write);
	}
}

void GPUTextureManager::FreeImageView(const RDescriptorIndex a_descriptor_index, const RDescriptorLayout a_global_layout, const DescriptorAllocation& a_allocation)
{
	OSAcquireSRWLockWrite(&m_lock);
	Vulkan::FreeViewImage(m_views[a_descriptor_index.handle]);
	m_views[a_descriptor_index.handle].index = m_next_free;
	m_next_free = m_views[a_descriptor_index.handle].index;
	OSReleaseSRWLockWrite(&m_lock);

	DescriptorWriteImageInfo write_info;
	write_info.binding = GLOBAL_BINDLESS_TEXTURES_BINDING;
	write_info.descriptor_index = a_descriptor_index.handle;
	write_info.view = GetImageView(s_render_inst->debug_descriptor_index);
	write_info.layout = IMAGE_LAYOUT::RO_FRAGMENT;
	write_info.allocation = a_allocation;
	write_info.descriptor_layout = a_global_layout;

	DescriptorWriteImage(write_info);
}

static void ImguiDisplayRenderer()
{
	if (ImGui::Begin("Renderer"))
	{
		s_render_inst->texture_manager.DisplayTextureListImgui();
		ImGui::InputFloat("gamma", &s_render_inst->global_buffer.data.gamma);
	}
	ImGui::End();
}

GPUBufferView BB::AllocateFromVertexBuffer(const size_t a_size_in_bytes)
{
	GPUBufferView view;
	view.buffer = s_render_inst->vertex_buffer.buffer;
	view.size = a_size_in_bytes;
	view.offset = s_render_inst->vertex_buffer.used.fetch_add(a_size_in_bytes);

	BB_ASSERT(s_render_inst->vertex_buffer.size > view.offset + a_size_in_bytes, "out of vertex buffer space!");
	return view;
}

GPUBufferView BB::AllocateFromIndexBuffer(const size_t a_size_in_bytes)
{
	GPUBufferView view;
	view.buffer = s_render_inst->index_buffer.buffer;
	view.size = a_size_in_bytes;
	view.offset = s_render_inst->index_buffer.used.fetch_add(a_size_in_bytes);

	BB_ASSERT(s_render_inst->index_buffer.size > view.offset + a_size_in_bytes, "out of index buffer space!");
	return view;
}

void BB::CopyToVertexBuffer(const RCommandList a_list, const GPUBuffer a_src, const Slice<RenderCopyBufferRegion> a_regions)
{
	RenderCopyBuffer copy_info;
	copy_info.src = a_src;
	copy_info.dst = s_render_inst->vertex_buffer.buffer;
	copy_info.regions = a_regions;

	Vulkan::CopyBuffer(a_list, copy_info);
}

void BB::CopyToIndexBuffer(const RCommandList a_list, const GPUBuffer a_src, const Slice<RenderCopyBufferRegion> a_regions)
{
	RenderCopyBuffer copy_info;
	copy_info.src = a_src;
	copy_info.dst = s_render_inst->index_buffer.buffer;
	copy_info.regions = a_regions;

	Vulkan::CopyBuffer(a_list, copy_info);
}

WriteableGPUBufferView BB::AllocateFromWritableVertexBuffer(const size_t a_size_in_bytes)
{
	WriteableGPUBufferView view;
	view.buffer = s_render_inst->cpu_vertex_buffer.buffer;
	view.size = a_size_in_bytes;
	view.offset = s_render_inst->cpu_vertex_buffer.used.fetch_add(static_cast<uint32_t>(a_size_in_bytes));
	view.mapped = Pointer::Add(s_render_inst->cpu_vertex_buffer.start_mapped, view.offset);

	BB_ASSERT(s_render_inst->cpu_vertex_buffer.size > view.offset + a_size_in_bytes, "out of vertex buffer space!");

	return view;
}

WriteableGPUBufferView BB::AllocateFromWritableIndexBuffer(const size_t a_size_in_bytes)
{
	WriteableGPUBufferView view;
	view.buffer = s_render_inst->cpu_index_buffer.buffer;
	view.size = a_size_in_bytes;
	view.offset = s_render_inst->cpu_index_buffer.used.fetch_add(static_cast<uint32_t>(a_size_in_bytes));
	view.mapped = Pointer::Add(s_render_inst->cpu_index_buffer.start_mapped, view.offset);

	BB_ASSERT(s_render_inst->cpu_index_buffer.size > view.offset + a_size_in_bytes, "out of index buffer space!");

	return view;
}

static GPUBuffer UploadStartupResources()
{
	CommandPool pool = GetGraphicsCommandPool();
	const RCommandList list = pool.StartCommandList();

	const uint32_t debug_texture_size = 4 * 4;

	GPUBufferCreateInfo create_info;
	create_info.host_writable = true;
	create_info.type = BUFFER_TYPE::UPLOAD;
	create_info.name = "startup resources upload";
	create_info.size = debug_texture_size + sizeof(SKYBOX_VERTICES);
	const GPUBuffer upload_buffer = CreateGPUBuffer(create_info);
	void* mapped = MapGPUBuffer(upload_buffer);

	{	// create debug texture
		const uint32_t debug_purple = (209u << 0u) | (106u << 8u) | (255u << 16u) | (255u << 24u);
		const uint32_t debug_black = 0;
		const uint32_t checker_board[4]{ debug_purple, debug_black, debug_black, debug_purple };

		memcpy(mapped, checker_board, sizeof(checker_board));

		ImageCreateInfo image_info;
		image_info.name = "debug texture";
		image_info.width = 2;
		image_info.height = 2;
		image_info.depth = 1;
		image_info.array_layers = 1;
		image_info.mip_levels = 1;
		image_info.type = IMAGE_TYPE::TYPE_2D;
		image_info.use_optimal_tiling = true;
		image_info.format = IMAGE_FORMAT::RGBA8_SRGB;
		image_info.usage = IMAGE_USAGE::TEXTURE;
		image_info.is_cube_map = false;
		s_render_inst->debug_texture = CreateImage(image_info);

		ImageViewCreateInfo debug_view_info;
		debug_view_info.name = "debug texture";
		debug_view_info.base_array_layer = 0;
		debug_view_info.mip_levels = 1;
		debug_view_info.array_layers = 1;
		debug_view_info.base_mip_level = 0;
		debug_view_info.type = IMAGE_VIEW_TYPE::TYPE_2D;
		debug_view_info.format = IMAGE_FORMAT::RGBA8_SRGB;
		debug_view_info.image = s_render_inst->debug_texture;
		debug_view_info.aspects = IMAGE_ASPECT::COLOR;
		s_render_inst->debug_descriptor_index = CreateImageView(debug_view_info);

		{
			PipelineBarrierImageInfo def_to_up;
			def_to_up.prev = IMAGE_LAYOUT::NONE;
			def_to_up.next = IMAGE_LAYOUT::COPY_DST;
			def_to_up.image = s_render_inst->debug_texture;
			def_to_up.layer_count = 1;
			def_to_up.level_count = 1;
			def_to_up.base_array_layer = 0;
			def_to_up.base_mip_level = 0;
			def_to_up.image_aspect = IMAGE_ASPECT::COLOR;

			PipelineBarrierInfo pipeline_info{};
			pipeline_info.image_barriers = ConstSlice<PipelineBarrierImageInfo>(&def_to_up, 1);
			PipelineBarriers(list, pipeline_info);
		}

		RenderCopyBufferToImageInfo write_to_image;
		write_to_image.src_buffer = upload_buffer;
		write_to_image.src_offset = 0;
		write_to_image.dst_image = s_render_inst->debug_texture;
		write_to_image.dst_aspects = IMAGE_ASPECT::COLOR;
		write_to_image.dst_image_info.extent = uint3(2, 2, 1);
		write_to_image.dst_image_info.offset = int3(0, 0, 0);
		write_to_image.dst_image_info.layer_count = 1;
		write_to_image.dst_image_info.mip_level = 0;
		write_to_image.dst_image_info.base_array_layer = 0;
		Vulkan::CopyBufferToImage(list, write_to_image);

		{
			PipelineBarrierImageInfo upl_to_shad;
			upl_to_shad.prev = IMAGE_LAYOUT::COPY_DST;
			upl_to_shad.next = IMAGE_LAYOUT::RO_FRAGMENT;
			upl_to_shad.image = s_render_inst->debug_texture;
			upl_to_shad.layer_count = 1;
			upl_to_shad.level_count = 1;
			upl_to_shad.base_array_layer = 0;
			upl_to_shad.base_mip_level = 0;
			upl_to_shad.image_aspect = IMAGE_ASPECT::COLOR;

			PipelineBarrierInfo pipeline_info{};
			pipeline_info.image_barriers = ConstSlice<PipelineBarrierImageInfo>(&upl_to_shad, 1);
			PipelineBarriers(list, pipeline_info);
		}
	}
	
	// setup cube positions
	{
		memcpy(Pointer::Add(mapped, debug_texture_size), SKYBOX_VERTICES, sizeof(SKYBOX_VERTICES));

		const GPUBufferView vertex_view = AllocateFromVertexBuffer(sizeof(SKYBOX_VERTICES));
		s_render_inst->global_buffer.data.cube_vertexpos_vertex_buffer_pos = static_cast<uint32_t>(vertex_view.offset);

		RenderCopyBufferRegion region;
		region.src_offset = debug_texture_size;
		region.dst_offset = 0;
		region.size = sizeof(SKYBOX_VERTICES);
		RenderCopyBuffer copy_buffer;
		copy_buffer.src = upload_buffer;
		copy_buffer.dst = vertex_view.buffer;
		copy_buffer.regions = Slice(&region, 1);

		CopyBuffer(list, copy_buffer);
	}

	pool.EndCommandList(list);

	UnmapGPUBuffer(upload_buffer);

	uint64_t a_out_fence_value;
	const bool success = ExecuteGraphicCommands(Slice(&pool, 1), nullptr, nullptr, 0, a_out_fence_value);
	BB_ASSERT(success, "failed to upload base resources");

	return upload_buffer;
}

bool BB::InitializeRenderer(MemoryArena& a_arena, const RendererCreateInfo& a_render_create_info)
{
	Vulkan::InitializeVulkan(a_arena, a_render_create_info);
	s_render_inst = ArenaAllocType(a_arena, RenderInterface_inst)(a_arena);
	s_render_inst->frame_count = BACK_BUFFER_MAX;
	s_render_inst->status.frame_index = 0;
	s_render_inst->frames = ArenaAllocArr(a_arena, RenderInterface_inst::Frame, BACK_BUFFER_MAX);
	Vulkan::CreateSwapchain(a_arena, a_render_create_info.window_handle, a_render_create_info.swapchain_width, a_render_create_info.swapchain_height, s_render_inst->frame_count);

	s_render_inst->debug = a_render_create_info.debug;

	s_render_inst->shader_effects.Init(a_arena, 64);

	s_render_inst->shader_compiler = CreateShaderCompiler(a_arena);

	MemoryArenaScope(a_arena)
	{
		{	//static sampler descriptor set 0
			FixedArray<SamplerCreateInfo, 2> immutable_samplers;
			immutable_samplers[0].name = "standard 3d sampler";
			immutable_samplers[0].mode_u = SAMPLER_ADDRESS_MODE::REPEAT;
			immutable_samplers[0].mode_v = SAMPLER_ADDRESS_MODE::REPEAT;
			immutable_samplers[0].mode_w = SAMPLER_ADDRESS_MODE::REPEAT;
			immutable_samplers[0].filter = SAMPLER_FILTER::LINEAR;
			immutable_samplers[0].max_anistoropy = 1.0f;
			immutable_samplers[0].max_lod = 100.f;
			immutable_samplers[0].min_lod = -100.f;
			immutable_samplers[0].border_color = SAMPLER_BORDER_COLOR::COLOR_FLOAT_OPAQUE_BLACK;

			immutable_samplers[1].name = "shadow map sampler";
			immutable_samplers[1].mode_u = SAMPLER_ADDRESS_MODE::CLAMP;
			immutable_samplers[1].mode_v = SAMPLER_ADDRESS_MODE::CLAMP;
			immutable_samplers[1].mode_w = SAMPLER_ADDRESS_MODE::CLAMP;
			immutable_samplers[1].filter = SAMPLER_FILTER::LINEAR;
			immutable_samplers[1].max_anistoropy = 1.0f;
			immutable_samplers[1].max_lod = 1.f;
			immutable_samplers[1].min_lod = 0.f;
			immutable_samplers[1].border_color = SAMPLER_BORDER_COLOR::COLOR_FLOAT_OPAQUE_WHITE;
			s_render_inst->static_sampler_descriptor_set = Vulkan::CreateDescriptorSamplerLayout(immutable_samplers.slice());
		}
		{
			//global descriptor set 1
			FixedArray<DescriptorBindingInfo, 4> descriptor_bindings;
			descriptor_bindings[0].binding = GLOBAL_VERTEX_BUFFER_BINDING;
			descriptor_bindings[0].count = 1;
			descriptor_bindings[0].shader_stage = SHADER_STAGE::VERTEX;
			descriptor_bindings[0].type = DESCRIPTOR_TYPE::READONLY_BUFFER;

			descriptor_bindings[1].binding = GLOBAL_CPU_VERTEX_BUFFER_BINDING;
			descriptor_bindings[1].count = 1;
			descriptor_bindings[1].shader_stage = SHADER_STAGE::VERTEX;
			descriptor_bindings[1].type = DESCRIPTOR_TYPE::READONLY_BUFFER;

			descriptor_bindings[2].binding = GLOBAL_BUFFER_BINDING;
			descriptor_bindings[2].count = 1;
			descriptor_bindings[2].shader_stage = SHADER_STAGE::ALL;
			descriptor_bindings[2].type = DESCRIPTOR_TYPE::READONLY_CONSTANT;

			descriptor_bindings[3].binding = GLOBAL_BINDLESS_TEXTURES_BINDING;
			descriptor_bindings[3].count = MAX_TEXTURES;
			descriptor_bindings[3].shader_stage = SHADER_STAGE::FRAGMENT_PIXEL;
			descriptor_bindings[3].type = DESCRIPTOR_TYPE::IMAGE;

			s_render_inst->global_descriptor_set = Vulkan::CreateDescriptorLayout(a_arena, descriptor_bindings.const_slice());
			s_render_inst->global_descriptor_allocation = Vulkan::AllocateDescriptor(s_render_inst->global_descriptor_set);
		}
	}

	{
		GPUBufferCreateInfo global_buffer;
		global_buffer.name = "global buffer";
		global_buffer.size = sizeof(GlobalRenderData);
		global_buffer.type = BUFFER_TYPE::UNIFORM;
		global_buffer.host_writable = true;

		s_render_inst->global_buffer.buffer = Vulkan::CreateBuffer(global_buffer);
		s_render_inst->global_buffer.mapped = Vulkan::MapBufferMemory(s_render_inst->global_buffer.buffer);
		s_render_inst->global_buffer.data.swapchain_resolution = uint2(a_render_create_info.swapchain_width, a_render_create_info.swapchain_height);
		s_render_inst->global_buffer.data.gamma = a_render_create_info.gamma;
	}

	{
		GPUBufferCreateInfo vertex_buffer;
		vertex_buffer.name = "global vertex buffer";
		vertex_buffer.size = mbSize * 64;
		vertex_buffer.type = BUFFER_TYPE::STORAGE; //using byteaddressbuffer to get the vertices
		vertex_buffer.host_writable = false;

		s_render_inst->vertex_buffer.buffer = Vulkan::CreateBuffer(vertex_buffer);
        s_render_inst->vertex_buffer.address = Vulkan::GetBufferAddress(s_render_inst->vertex_buffer.buffer);
		s_render_inst->vertex_buffer.size = static_cast<uint32_t>(vertex_buffer.size);
		s_render_inst->vertex_buffer.used = 0;

		vertex_buffer.host_writable = true;
		s_render_inst->cpu_vertex_buffer.buffer = Vulkan::CreateBuffer(vertex_buffer);
        s_render_inst->cpu_vertex_buffer.address = Vulkan::GetBufferAddress(s_render_inst->cpu_vertex_buffer.buffer);
		s_render_inst->cpu_vertex_buffer.size = static_cast<uint32_t>(vertex_buffer.size);
		s_render_inst->cpu_vertex_buffer.used = 0;
		s_render_inst->cpu_vertex_buffer.start_mapped = Vulkan::MapBufferMemory(s_render_inst->cpu_vertex_buffer.buffer);

		{
			DescriptorWriteBufferInfo buffer_info;
			buffer_info.descriptor_layout = s_render_inst->global_descriptor_set;
			buffer_info.descriptor_index = 0;
			buffer_info.allocation = s_render_inst->global_descriptor_allocation;
			buffer_info.binding = GLOBAL_VERTEX_BUFFER_BINDING;
			buffer_info.buffer_view.buffer = s_render_inst->vertex_buffer.buffer;
			buffer_info.buffer_view.offset = 0;
			buffer_info.buffer_view.size = s_render_inst->vertex_buffer.size;
			DescriptorWriteStorageBuffer(buffer_info);
		}
		{
			DescriptorWriteBufferInfo buffer_info;
			buffer_info.descriptor_layout = s_render_inst->global_descriptor_set;
			buffer_info.descriptor_index = 0;
			buffer_info.allocation = s_render_inst->global_descriptor_allocation;
			buffer_info.binding = GLOBAL_CPU_VERTEX_BUFFER_BINDING;
			buffer_info.buffer_view.buffer = s_render_inst->cpu_vertex_buffer.buffer;
			buffer_info.buffer_view.offset = 0;
			buffer_info.buffer_view.size = s_render_inst->cpu_vertex_buffer.size;
			DescriptorWriteStorageBuffer(buffer_info);
		}
		{
			DescriptorWriteBufferInfo buffer_info;
			buffer_info.descriptor_layout = s_render_inst->global_descriptor_set;
			buffer_info.descriptor_index = 0;
			buffer_info.allocation = s_render_inst->global_descriptor_allocation;
			buffer_info.binding = GLOBAL_BUFFER_BINDING;
			buffer_info.buffer_view.buffer = s_render_inst->global_buffer.buffer;
			buffer_info.buffer_view.offset = 0;
			buffer_info.buffer_view.size = sizeof(s_render_inst->global_buffer.data);
			DescriptorWriteUniformBuffer(buffer_info);
		}
	}
	{
		GPUBufferCreateInfo index_buffer;
		index_buffer.name = "global index buffer";
		index_buffer.size = mbSize * 64;
		index_buffer.type = BUFFER_TYPE::INDEX;
		index_buffer.host_writable = false;

		s_render_inst->index_buffer.buffer = Vulkan::CreateBuffer(index_buffer);
        s_render_inst->index_buffer.address = Vulkan::GetBufferAddress(s_render_inst->index_buffer.buffer);
		s_render_inst->index_buffer.size = static_cast<uint32_t>(index_buffer.size);
		s_render_inst->index_buffer.used = 0;

		index_buffer.host_writable = true;
		s_render_inst->cpu_index_buffer.buffer = Vulkan::CreateBuffer(index_buffer);
        s_render_inst->cpu_index_buffer.address = Vulkan::GetBufferAddress(s_render_inst->cpu_index_buffer.buffer);
		s_render_inst->cpu_index_buffer.size = static_cast<uint32_t>(index_buffer.size);
		s_render_inst->cpu_index_buffer.used = 0;
		s_render_inst->cpu_index_buffer.start_mapped = Vulkan::MapBufferMemory(s_render_inst->cpu_index_buffer.buffer);
	}


	s_render_inst->texture_manager.Init(a_arena);
	const GPUBuffer startup_buffer = UploadStartupResources();
	s_render_inst->texture_manager.SetAllTextures(s_render_inst->debug_descriptor_index, s_render_inst->global_descriptor_set, s_render_inst->global_descriptor_allocation);
	GPUWaitIdle();

	FreeGPUBuffer(startup_buffer);

	return true;
}

bool BB::DestroyRenderer()
{
	BB_UNIMPLEMENTED("BB::DestroyRenderer");
	// delete all vulkan objects
	return true;
}

void BB::GPUWaitIdle()
{
	s_render_inst->transfer_queue.WaitIdle();
	s_render_inst->graphics_queue.WaitIdle();
	s_render_inst->compute_queue.WaitIdle();
}

GPUDeviceInfo BB::GetGPUInfo(MemoryArena& a_arena)
{
	return Vulkan::GetGPUDeviceInfo(a_arena);
}

uint32_t BB::GetBackBufferCount()
{
	return s_render_inst->frame_count;
}

void BB::RenderStartFrame(const RCommandList a_list, const RenderStartFrameInfo& a_info, const RImage a_render_target, uint32_t& a_back_buffer_index)
{
	BB_ASSERT(s_render_inst->status.frame_started == false, "did not call RenderEndFrame before a new RenderStartFrame");
	s_render_inst->status.frame_started = true;

	const uint32_t frame_index = s_render_inst->status.frame_index;
	const RenderInterface_inst::Frame& cur_frame = s_render_inst->frames[frame_index];

	s_render_inst->graphics_queue.WaitFenceValue(cur_frame.graphics_queue_fence_value);

	{
		PipelineBarrierImageInfo image_transitions[1]{};
		image_transitions[0].prev = IMAGE_LAYOUT::NONE;
		image_transitions[0].next = IMAGE_LAYOUT::RT_COLOR;
		image_transitions[0].image = a_render_target;
		image_transitions[0].layer_count = 1;
		image_transitions[0].level_count = 1;
		image_transitions[0].base_array_layer = static_cast<uint16_t>(frame_index);
		image_transitions[0].base_mip_level = 0;
		image_transitions[0].image_aspect = IMAGE_ASPECT::COLOR;

		PipelineBarrierInfo pipeline_info{};
		pipeline_info.image_barriers = ConstSlice<PipelineBarrierImageInfo>(image_transitions, 1);
		Vulkan::PipelineBarriers(a_list, pipeline_info);
	}

	{
		s_render_inst->global_buffer.data.frame_count += 1;
		s_render_inst->global_buffer.data.mouse_pos = a_info.mouse_pos;
		s_render_inst->global_buffer.data.frame_index = s_render_inst->status.frame_index;
		s_render_inst->global_buffer.data.delta_time = a_info.delta_time;
		s_render_inst->global_buffer.data.total_time += a_info.delta_time;
		memcpy(s_render_inst->global_buffer.mapped,
			&s_render_inst->global_buffer.data,
			sizeof(s_render_inst->global_buffer.data));
	}

	ImguiDisplayRenderer();
	a_back_buffer_index = frame_index;
}

PRESENT_IMAGE_RESULT BB::RenderEndFrame(const RCommandList a_list, const RImage a_render_target, const uint32_t a_render_target_layer)
{
	BB_ASSERT(s_render_inst->status.frame_started == true, "did not call RenderStartFrame before a RenderEndFrame");

	PipelineBarrierImageInfo image_transitions[1]{};
	image_transitions[0].prev = IMAGE_LAYOUT::RT_COLOR;
	image_transitions[0].next = IMAGE_LAYOUT::COPY_SRC;
	image_transitions[0].image = a_render_target;
	image_transitions[0].layer_count = 1;
	image_transitions[0].level_count = 1;
	image_transitions[0].base_array_layer = static_cast<uint16_t>(a_render_target_layer);
	image_transitions[0].base_mip_level = 0;
	image_transitions[0].image_aspect = IMAGE_ASPECT::COLOR;

	PipelineBarrierInfo pipeline_info{};
	pipeline_info.image_barriers = ConstSlice<PipelineBarrierImageInfo>(image_transitions, 1);
	Vulkan::PipelineBarriers(a_list, pipeline_info);

	const int2 swapchain_size(static_cast<int>(s_render_inst->global_buffer.data.swapchain_resolution.x), static_cast<int>(s_render_inst->global_buffer.data.swapchain_resolution.y));

	const PRESENT_IMAGE_RESULT result = Vulkan::UploadImageToSwapchain(a_list, a_render_target, a_render_target_layer, swapchain_size, swapchain_size, s_render_inst->status.frame_index);
	s_render_inst->status.frame_ended = true;

	return result;
}

bool BB::ResizeSwapchain(const uint2 a_extent)
{
	s_render_inst->global_buffer.data.swapchain_resolution = a_extent;
	return Vulkan::RecreateSwapchain(a_extent.x, a_extent.y);
}

void BB::StartRenderPass(const RCommandList a_list, const StartRenderingInfo& a_render_info)
{
	Vulkan::StartRenderPass(a_list, a_render_info);
}

void BB::EndRenderPass(const RCommandList a_list)
{
	Vulkan::EndRenderPass(a_list);
}

void BB::BindIndexBuffer(const RCommandList a_list, const uint64_t a_offset, const bool a_cpu_readable)
{
	if (a_cpu_readable)
	{
		Vulkan::BindIndexBuffer(a_list, s_render_inst->cpu_index_buffer.buffer, a_offset);
	}
	else
	{
		Vulkan::BindIndexBuffer(a_list, s_render_inst->index_buffer.buffer, a_offset);
	}
}

RPipelineLayout BB::BindShaders(const RCommandList a_list, const ConstSlice<ShaderEffectHandle> a_shader_effects)
{
	constexpr size_t SHADERS_MAX = 5;
	BB_ASSERT(SHADERS_MAX >= a_shader_effects.size(), "binding more then 5 shaders at a time, this is wrong or increase SHADERS_MAX");

	ShaderObject shader_objects[SHADERS_MAX]{};
	SHADER_STAGE shader_stages[SHADERS_MAX]{};

	SHADER_STAGE_FLAGS next_supported_stages = static_cast<uint32_t>(SHADER_STAGE::ALL);
	RPipelineLayout layout{};
	for (size_t eff_index = 0; eff_index < a_shader_effects.size(); eff_index++)
	{
		const ShaderEffect& effect = s_render_inst->shader_effects[a_shader_effects[eff_index]];
		shader_objects[eff_index] = effect.shader_object;
		shader_stages[eff_index] = effect.shader_stage;
		
		BB_ASSERT((next_supported_stages & static_cast<SHADER_STAGE_FLAGS>(effect.shader_stage)) == static_cast<SHADER_STAGE_FLAGS>(effect.shader_stage),
			"shader does not support the next stage");

		if (layout.IsValid())
			BB_ASSERT(layout == effect.pipeline_layout, "pipeline layout is wrong");
		else
			layout = effect.pipeline_layout;

		next_supported_stages = effect.shader_stages_next;
	}

	Vulkan::BindShaders(a_list, static_cast<uint32_t>(a_shader_effects.size()), shader_stages, shader_objects);
	// set the samplers
	Vulkan::SetDescriptorImmutableSamplers(a_list, layout);

	return layout;
}

void BB::SetBlendMode(const RCommandList a_list, const uint32_t a_first_attachment, const Slice<ColorBlendState> a_blend_states)
{
	Vulkan::SetBlendMode(a_list, a_first_attachment, a_blend_states);
}

void BB::SetFrontFace(const RCommandList a_list, const bool a_is_clockwise)
{
	Vulkan::SetFrontFace(a_list, a_is_clockwise);
}

void BB::SetCullMode(const RCommandList a_list, const CULL_MODE a_cull_mode)
{
	Vulkan::SetCullMode(a_list, a_cull_mode);
}

void BB::SetDepthBias(const RCommandList a_list, const float a_bias_constant_factor, const float a_bias_clamp, const float a_bias_slope_factor)
{
	Vulkan::SetDepthBias(a_list, a_bias_constant_factor, a_bias_clamp, a_bias_slope_factor);
}

void BB::SetScissor(const RCommandList a_list, const ScissorInfo& a_scissor)
{
	Vulkan::SetScissor(a_list, a_scissor);
}

void BB::DrawVertices(const RCommandList a_list, const uint32_t a_vertex_count, const uint32_t a_instance_count, const uint32_t a_first_vertex, const uint32_t a_first_instance)
{
	Vulkan::DrawVertices(a_list, a_vertex_count, a_instance_count, a_first_vertex, a_first_instance);
}

void BB::DrawCubemap(const RCommandList a_list, const uint32_t a_instance_count, const uint32_t a_first_instance)
{
	Vulkan::DrawVertices(a_list, _countof(SKYBOX_VERTICES), a_instance_count, 0, a_first_instance);
}

void BB::DrawIndexed(const RCommandList a_list, const uint32_t a_index_count, const uint32_t a_instance_count, const uint32_t a_first_index, const int32_t a_vertex_offset, const uint32_t a_first_instance)
{
	Vulkan::DrawIndexed(a_list, a_index_count, a_instance_count, a_first_index, a_vertex_offset, a_first_instance);
}

CommandPool& BB::GetGraphicsCommandPool()
{
	return s_render_inst->graphics_queue.GetCommandPool();
}

CommandPool& BB::GetTransferCommandPool()
{
	return s_render_inst->transfer_queue.GetCommandPool();
}

CommandPool& BB::GetCommandCommandPool()
{
	return s_render_inst->compute_queue.GetCommandPool();
}

PRESENT_IMAGE_RESULT BB::PresentFrame(const BB::Slice<CommandPool> a_cmd_pools, const RFence* a_signal_fences, const uint64_t* a_signal_values, const uint32_t a_signal_count, uint64_t& a_out_present_fence_value, const bool a_skip)
{
	if (a_skip)
	{
		s_render_inst->graphics_queue.ReturnPools(a_cmd_pools);
		s_render_inst->status.frame_ended = false;
		s_render_inst->status.frame_started = false;
		return PRESENT_IMAGE_RESULT::SKIPPED;
	}

	BB_ASSERT(s_render_inst->status.frame_started == true, "did not call RenderStartFrame before a presenting");
	BB_ASSERT(s_render_inst->status.frame_ended == true, "did not call RenderEndFrame before a presenting");

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

	//set the next fence value for the frame
	s_render_inst->graphics_queue.ReturnPools(a_cmd_pools);
	const PRESENT_IMAGE_RESULT result = s_render_inst->graphics_queue.ExecutePresentCommands(lists, list_count, a_signal_fences, a_signal_values, a_signal_count, nullptr, nullptr, 0, s_render_inst->status.frame_index, a_out_present_fence_value);
	s_render_inst->status.frame_index = (s_render_inst->status.frame_index + 1) % s_render_inst->frame_count;
	s_render_inst->frames[s_render_inst->status.frame_index].graphics_queue_fence_value = a_out_present_fence_value;

	s_render_inst->status.frame_ended = false;
	s_render_inst->status.frame_started = false;

	return result;
}

static bool ExecuteQueueCommands(RenderQueue& a_queue, const BB::Slice<CommandPool> a_cmd_pools, const RFence* a_signal_fences, const uint64_t* a_signal_values, const uint32_t a_signal_count, uint64_t& a_out_present_fence_value)
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

	a_queue.ReturnPools(a_cmd_pools);
	a_queue.ExecuteCommands(lists, list_count, a_signal_fences, a_signal_values, a_signal_count, nullptr, nullptr, 0, a_out_present_fence_value);
	return true;
}

bool BB::ExecuteGraphicCommands(const BB::Slice<CommandPool> a_cmd_pools, const RFence* a_signal_fences, const uint64_t* a_signal_values, const uint32_t a_signal_count, uint64_t& a_out_present_fence_value)
{
	return ExecuteQueueCommands(s_render_inst->graphics_queue, a_cmd_pools, a_signal_fences, a_signal_values, a_signal_count, a_out_present_fence_value);
}

bool BB::ExecuteTransferCommands(const BB::Slice<CommandPool> a_cmd_pools, const RFence* a_signal_fences, const uint64_t* a_signal_values, const uint32_t a_signal_count, uint64_t& a_out_present_fence_value)
{
	return ExecuteQueueCommands(s_render_inst->transfer_queue, a_cmd_pools, a_signal_fences, a_signal_values, a_signal_count, a_out_present_fence_value);
}

bool BB::ExecuteComputeCommands(const BB::Slice<CommandPool> a_cmd_pools, const RFence* a_signal_fences, const uint64_t* a_signal_values, const uint32_t a_signal_count, uint64_t& a_out_present_fence_value)
{
	return ExecuteQueueCommands(s_render_inst->compute_queue, a_cmd_pools, a_signal_fences, a_signal_values, a_signal_count, a_out_present_fence_value);
}

RDescriptorLayout BB::CreateDescriptorLayout(MemoryArena& a_temp_arena, const ConstSlice<DescriptorBindingInfo> a_bindings)
{
	return Vulkan::CreateDescriptorLayout(a_temp_arena, a_bindings);
}

DescriptorAllocation BB::AllocateDescriptor(const RDescriptorLayout a_descriptor)
{
	return Vulkan::AllocateDescriptor(a_descriptor);
}

bool BB::CreateShaderEffect(MemoryArena& a_temp_arena, const Slice<CreateShaderEffectInfo> a_create_infos, ShaderEffectHandle* const a_handles, bool a_link_shaders)
{
	// clean this all up, tons of duplication going on.

	ShaderEffect* shader_effects = ArenaAllocArr(a_temp_arena, ShaderEffect, a_create_infos.size());
	ShaderCode* shader_codes = ArenaAllocArr(a_temp_arena, ShaderCode, a_create_infos.size());
	ShaderObjectCreateInfo* shader_object_infos = ArenaAllocArr(a_temp_arena, ShaderObjectCreateInfo, a_create_infos.size());

	for (size_t i = 0; i < a_create_infos.size(); i++)
	{
		const CreateShaderEffectInfo& create_info = a_create_infos[i];

		PushConstantRange push_constant_range;
		push_constant_range.stages = SHADER_STAGE::ALL;
		push_constant_range.size = create_info.push_constant_space;

		shader_effects[i].pipeline_layout = Vulkan::CreatePipelineLayout(create_info.desc_layouts.data(), create_info.desc_layout_count, push_constant_range);

		if (!CompileShader(s_render_inst->shader_compiler,
			create_info.shader_data,
			create_info.shader_entry,
			create_info.stage,
			shader_codes[i]))
		{
			BB_WARNING(false, "failed to compile shader and aborting shader object creation", WarningType::HIGH);
			return false;
		}
		const Buffer shader_buffer = GetShaderCodeBuffer(shader_codes[i]);

		shader_object_infos[i].stage = create_info.stage;
		shader_object_infos[i].next_stages = create_info.next_stages;
		shader_object_infos[i].shader_code_size = shader_buffer.size;
		shader_object_infos[i].shader_code = shader_buffer.data;
		shader_object_infos[i].shader_entry = create_info.shader_entry;

		shader_object_infos[i].descriptor_layout_count = create_info.desc_layout_count;
		shader_object_infos[i].descriptor_layouts = create_info.desc_layouts;
		shader_object_infos[i].push_constant_range = push_constant_range;
	}

	ShaderObject* shader_objects = ArenaAllocArr(a_temp_arena, ShaderObject, a_create_infos.size());
	Vulkan::CreateShaderObjects(a_temp_arena, Slice(shader_object_infos, a_create_infos.size()), shader_objects, a_link_shaders);

	for (size_t i = 0; i < a_create_infos.size(); i++)
	{
		const CreateShaderEffectInfo& create_info = a_create_infos[i];

		shader_effects[i].name = create_info.name;
		shader_effects[i].shader_object = shader_objects[i];
		shader_effects[i].shader_stage = create_info.stage;
		shader_effects[i].shader_stages_next = create_info.next_stages;
#ifdef _ENABLE_REBUILD_SHADERS
		shader_effects[i].create_info = shader_object_infos[i];
		shader_effects[i].shader_entry = create_info.shader_entry;
#endif // _ENABLE_REBUILD_SHADERS

		a_handles[i] = s_render_inst->shader_effects.emplace(shader_effects[i]);
		ReleaseShaderCode(shader_codes[i]);
	}
	return true;
}
// TODO actually remove shaders
//void BB::FreeShaderEffect(const ShaderEffectHandle a_shader_effect)
//{
//	ShaderEffect shader_effect = s_render_inst->shader_effect_map.find(a_shader_effect);
//	Vulkan::DestroyShaderObject(shader_effect.shader_object);
//	Vulkan::FreePipelineLayout(shader_effect.pipeline_layout);
//	s_render_inst->shader_effect_map.erase(a_shader_effect);
//}

bool BB::ReloadShaderEffect(const ShaderEffectHandle a_shader_effect, const Buffer& a_shader)
{
#ifdef _ENABLE_REBUILD_SHADERS
	GPUWaitIdle();
	ShaderEffect& old_effect = s_render_inst->shader_effects[a_shader_effect];

	ShaderCode shader_code;
	if (!CompileShader(s_render_inst->shader_compiler,
		a_shader,
		old_effect.shader_entry,
		old_effect.shader_stage,
		shader_code))
	{
		return false;
	}

	const Buffer shader_data = GetShaderCodeBuffer(shader_code);
	old_effect.create_info.shader_code = shader_data.data;
	old_effect.create_info.shader_code_size = shader_data.size;
	old_effect.create_info.shader_entry = old_effect.shader_entry;

	const ShaderObject new_object = Vulkan::CreateShaderObject(old_effect.create_info);
	ReleaseShaderCode(shader_code);
	if (new_object.IsValid())
	{
		Vulkan::DestroyShaderObject(old_effect.shader_object);
		old_effect.shader_object = new_object;
		return true;
	}
	else
		return false;

#endif // _ENABLE_REBUILD_SHADERS
	BB_WARNING(false, "trying to reload a shader but _ENABLE_REBUILD_SHADERS is not defined", WarningType::MEDIUM);
	return true;
}

const RImage BB::CreateImage(const ImageCreateInfo& a_create_info)
{
	return Vulkan::CreateImage(a_create_info);
}

const RDescriptorIndex BB::CreateImageView(const ImageViewCreateInfo& a_create_info)
{
	return s_render_inst->texture_manager.AllocAndWriteImageView(a_create_info, s_render_inst->global_descriptor_set, s_render_inst->global_descriptor_allocation);
}

const RImageView BB::CreateImageViewShaderInaccessible(const ImageViewCreateInfo& a_create_info)
{
	return Vulkan::CreateImageView(a_create_info);
}

const RImageView BB::GetImageView(const RDescriptorIndex a_index)
{
	return s_render_inst->texture_manager.GetImageView(a_index);
}

void BB::FreeImage(const RImage a_image)
{
	Vulkan::FreeImage(a_image);
}

void BB::FreeImageView(const RDescriptorIndex a_index)
{
	s_render_inst->texture_manager.FreeImageView(a_index, s_render_inst->global_descriptor_set, s_render_inst->global_descriptor_allocation);
}

void BB::FreeImageViewShaderInaccessible(const RImageView a_view)
{
	Vulkan::FreeViewImage(a_view);
}

void BB::ClearImage(const RCommandList a_list, const ClearImageInfo& a_clear_info)
{
	Vulkan::ClearImage(a_list, a_clear_info);
}

void BB::ClearDepthImage(const RCommandList a_list, const ClearDepthImageInfo& a_clear_info)
{
	Vulkan::ClearDepthImage(a_list, a_clear_info);
}

void BB::BlitImage(const RCommandList a_list, const BlitImageInfo& a_blit_info)
{
	// use with new asset system, but this is not used now.
	BB_UNIMPLEMENTED("BB::BlitImage");

	Vulkan::BlitImage(a_list, a_blit_info);
}

void BB::CopyImage(const RCommandList a_list, const CopyImageInfo& a_copy_info)
{
	// use with new asset system, but this is not used.
	BB_UNIMPLEMENTED("BB::CopyImage");

	Vulkan::CopyImage(a_list, a_copy_info);
}

void BB::CopyBufferToImage(const RCommandList a_list, const RenderCopyBufferToImageInfo& a_copy_info)
{
	Vulkan::CopyBufferToImage(a_list, a_copy_info);
}

void BB::CopyImageToBuffer(const RCommandList a_list, const RenderCopyImageToBufferInfo& a_copy_info)
{
	Vulkan::CopyImageToBuffer(a_list, a_copy_info);
}

const GPUBuffer BB::CreateGPUBuffer(const GPUBufferCreateInfo& a_create_info)
{
	return Vulkan::CreateBuffer(a_create_info);
}

void BB::FreeGPUBuffer(const GPUBuffer a_buffer)
{
	Vulkan::FreeBuffer(a_buffer);
}

void* BB::MapGPUBuffer(const GPUBuffer a_buffer)
{
	return Vulkan::MapBufferMemory(a_buffer);
}

void BB::UnmapGPUBuffer(const GPUBuffer a_buffer)
{
	Vulkan::UnmapBufferMemory(a_buffer);
}

GPUAddress BB::GetGPUBufferAddress(const GPUBuffer a_buffer)
{
	return Vulkan::GetBufferAddress(a_buffer);
}

void BB::CopyBuffer(const RCommandList a_list, const RenderCopyBuffer& a_copy_buffer)
{
	Vulkan::CopyBuffer(a_list, a_copy_buffer);
}

size_t BB::AccelerationStructureInstanceUploadSize()
{
	return Vulkan::AccelerationStructureInstanceUploadSize();
}

bool BB::UploadAccelerationStructureInstances(void* a_mapped, const size_t a_mapped_size, const ConstSlice<AccelerationStructureInstanceInfo> a_instances)
{
	return Vulkan::UploadAccelerationStructureInstances(a_mapped, a_mapped_size, a_instances);
}

AccelerationStructSizeInfo BB::GetBottomLevelAccelerationStructSizeInfo(MemoryArena& a_temp_arena, const ConstSlice<AccelerationStructGeometrySize> a_geometry_sizes, const ConstSlice<uint32_t> a_primitive_counts)
{
    return Vulkan::GetBottomLevelAccelerationStructSizeInfo(a_temp_arena, a_geometry_sizes, a_primitive_counts, s_render_inst->vertex_buffer.address, s_render_inst->index_buffer.address);
}

AccelerationStructSizeInfo BB::GetTopLevelAccelerationStructSizeInfo(MemoryArena& a_temp_arena, const ConstSlice<GPUAddress> a_instances)
{
    return Vulkan::GetTopLevelAccelerationStructSizeInfo(a_temp_arena, a_instances);
}

RAccelerationStruct BB::CreateBottomLevelAccelerationStruct(const uint32_t a_acceleration_structure_size, const GPUBuffer a_dst_buffer, const uint64_t a_dst_offset)
{
	return Vulkan::CreateBottomLevelAccelerationStruct(a_acceleration_structure_size, a_dst_buffer, a_dst_offset);
}

RAccelerationStruct BB::CreateTopLevelAccelerationStruct(const uint32_t a_acceleration_structure_size, const GPUBuffer a_dst_buffer, const uint64_t a_dst_offset)
{
	return Vulkan::CreateTopLevelAccelerationStruct(a_acceleration_structure_size, a_dst_buffer, a_dst_offset);
}

GPUAddress BB::GetAccelerationStructureAddress(const RAccelerationStruct a_acc_struct)
{
    return Vulkan::GetAccelerationStructureAddress(a_acc_struct);
}

void BB::BuildBottomLevelAccelerationStruct(MemoryArena& a_temp_arena, const RCommandList a_list, const BuildBottomLevelAccelerationStructInfo& a_build_info)
{
    Vulkan::BuildBottomLevelAccelerationStruct(a_temp_arena, a_list, a_build_info, s_render_inst->vertex_buffer.address, s_render_inst->index_buffer.address);
}

void BB::BuildTopLevelAccelerationStruct(MemoryArena& a_temp_arena, const RCommandList a_list, const BuildTopLevelAccelerationStructInfo& a_build_info)
{
    Vulkan::BuildTopLevelAccelerationStruct(a_temp_arena, a_list, a_build_info);
}

void BB::DescriptorWriteUniformBuffer(const DescriptorWriteBufferInfo& a_write_info)
{
	Vulkan::DescriptorWriteUniformBuffer(a_write_info);
}

void BB::DescriptorWriteStorageBuffer(const DescriptorWriteBufferInfo& a_write_info)
{
	Vulkan::DescriptorWriteStorageBuffer(a_write_info);
}

void BB::DescriptorWriteImage(const DescriptorWriteImageInfo& a_write_info)
{
	Vulkan::DescriptorWriteImage(a_write_info);
}

RFence BB::CreateFence(const uint64_t a_initial_value, const char* a_name)
{
	return Vulkan::CreateFence(a_initial_value, a_name);
}

void BB::FreeFence(const RFence a_fence)
{
	Vulkan::FreeFence(a_fence);
}

void BB::WaitFence(const RFence a_fence, const GPUFenceValue a_fence_value)
{
	Vulkan::WaitFence(a_fence, a_fence_value);
}

void BB::WaitFences(const RFence* a_fences, const GPUFenceValue* a_fence_values, const uint32_t a_fence_count)
{
	Vulkan::WaitFences(a_fences, a_fence_values, a_fence_count);
}

GPUFenceValue BB::GetCurrentFenceValue(const RFence a_fence)
{
	return Vulkan::GetCurrentFenceValue(a_fence);
}

void BB::SetPushConstants(const RCommandList a_list, const RPipelineLayout a_pipe_layout, const uint32_t a_offset, const uint32_t a_size, const void* a_data)
{
	Vulkan::SetPushConstants(a_list, a_pipe_layout, a_offset, a_size, a_data);
}

void BB::PipelineBarriers(const RCommandList a_list, const PipelineBarrierInfo& a_barrier_info)
{
	Vulkan::PipelineBarriers(a_list, a_barrier_info);
}

void BB::SetDescriptorBufferOffset(const RCommandList a_list, const RPipelineLayout a_pipe_layout, const uint32_t a_first_set, const uint32_t a_set_count, const uint32_t* a_buffer_indices, const size_t* a_offsets)
{
	Vulkan::SetDescriptorBufferOffset(a_list, a_pipe_layout, a_first_set, a_set_count, a_buffer_indices, a_offsets);
}

const BB::DescriptorAllocation& BB::GetGlobalDescriptorAllocation()
{
	return s_render_inst->global_descriptor_allocation;
}

RDescriptorIndex BB::GetDebugTexture()
{
	return s_render_inst->debug_descriptor_index;
}

RDescriptorLayout BB::GetStaticSamplerDescriptorLayout()
{
	return s_render_inst->static_sampler_descriptor_set;
}

RDescriptorLayout BB::GetGlobalDescriptorLayout()
{
	return s_render_inst->global_descriptor_set;
}
