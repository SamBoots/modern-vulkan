#include "Renderer.hpp"
#include "VulkanRenderer.hpp"

#include "Storage/Slotmap.h"
#include "Storage/Queue.hpp"
#include "Program.h"

#include "ShaderCompiler.h"

#include "Math.inl"

#include "imgui.h"

using namespace BB;

struct RenderFence
{
	uint64_t next_fence_value;
	uint64_t last_complete_value;
	RFence fence;
};

constexpr uint32_t MAX_TEXTURES = 1024;
constexpr const char* DEBUG_TEXTURE_NAME = "debug texture";

static IMAGE_FORMAT DEPTH_IMAGE_FORMAT;
constexpr IMAGE_FORMAT RENDER_TARGET_IMAGE_FORMAT = IMAGE_FORMAT::RGBA8_SRGB;

class GPUTextureManager
{
public:
	struct TextureInfo
	{
		RImage image;				// 8
		RImageView view;			// 16

		uint32_t width;				// 20
		uint32_t height;			// 24
		IMAGE_FORMAT format;		// 28
		IMAGE_LAYOUT current_layout;// 32
	};

	struct TextureSlot
	{
		TextureInfo texture_info;	// 28
		const char* name;			// 36
		uint32_t next_free;			// 40
	};

	void Init(MemoryArena& a_arena, const RCommandList a_list, const uint64_t a_upload_fence_value);

	const RTexture UploadTexture(const RCommandList a_list, const UploadTextureInfo& a_upload_info, const uint64_t a_upload_fence_value);
	const RTexture CreateRenderTarget(const uint2 a_render_target_extent, const char* a_name);
	const RTexture CreateUploadTarget(const uint2 a_extent, const char* a_name);
	const RTexture CreateDepthImage(const uint2 a_depth_image_extent, const char* a_name);
	void FreeTexture(const RTexture a_texture);

	TextureSlot& GetTextureSlot(const RTexture a_texture)
	{
		return m_textures[a_texture.handle];
	}

	void TransitionGraphicsQueueTexture(const RCommandList a_list)
	{
		if (m_graphics_texture_transitions.size() == 0)
			return;
		OSAcquireSRWLockWrite(&m_lock);
		PipelineBarrierInfo barrier_info{};
		barrier_info.image_infos = m_graphics_texture_transitions.data();
		barrier_info.image_info_count = m_graphics_texture_transitions.size();
		Vulkan::PipelineBarriers(a_list, barrier_info);
		m_graphics_texture_transitions.clear();
		OSReleaseSRWLockWrite(&m_lock);
	}

	void DisplayTextureListImgui()
	{
		if (ImGui::CollapsingHeader("Texture Manager"))
		{
			ImGui::Indent();
			if (ImGui::CollapsingHeader("next free image slot"))
			{
				ImGui::Indent();
				ImGui::Text("Texture slot index: %u", m_next_free);
				const ImVec2 image_size = { 160, 160 };
				ImGui::Image(m_next_free, image_size);
				ImGui::Unindent();
			}

			for (uint32_t i = 0; i < MAX_TEXTURES; i++)
			{
				const TextureSlot& slot = m_textures[i];
				const size_t id = i;
				if (ImGui::TreeNodeEx(reinterpret_cast<void*>(id), 0, "Texture Slot: %u", i))
				{
					ImGui::Indent();
					if (slot.name != nullptr)
						ImGui::TextUnformatted(slot.name);
					else
						ImGui::Text("UNNAMED! This might be an error");
					const ImVec2 image_size = { 160, 160 };
					ImGui::Image(i, image_size);
					ImGui::Unindent();
					ImGui::TreePop();
				}
			}
			ImGui::Unindent();
		}
	}

	RTexture GetDebugTexture() const { return RTexture(0); }

private:
	uint32_t m_next_free;
	TextureSlot m_textures[MAX_TEXTURES];
	BBRWLock m_lock;

	StaticArray<PipelineBarrierImageInfo> m_graphics_texture_transitions;

	//purple color
	TextureInfo m_debug_texture;
};

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

struct UploadBuffer
{
	void SafeMemcpy(const size_t a_dest_offset, const void* a_src, const size_t a_src_size) const
	{
		void* copy_pos = Pointer::Add(begin, a_dest_offset);
		BB_ASSERT(Pointer::Add(copy_pos, a_src_size) <= end, "gpu upload buffer writing out of bounds");

		memcpy(copy_pos, a_src, a_src_size);
	}

	GPUBuffer buffer;
	void* begin;
	void* end;
	size_t base_offset;
};

constexpr size_t RING_BUFFER_QUEUE_ELEMENT_COUNT = 1024;
class UploadRingAllocator
{
public:
	void Init(MemoryArena& a_arena, const size_t a_ring_buffer_size, const RFence a_fence, const char* a_name)
	{
		GPUBufferCreateInfo create_info;
		create_info.type = BUFFER_TYPE::UPLOAD;
		create_info.size = a_ring_buffer_size;
		create_info.name = a_name;
		create_info.host_writable = true;

		m_lock = OSCreateRWLock();
		m_fence = a_fence;
		m_buffer = Vulkan::CreateBuffer(create_info);
		m_begin = Vulkan::MapBufferMemory(m_buffer);
		m_write_at = m_begin;
		m_end = Pointer::Add(m_begin, a_ring_buffer_size);
		m_free_until = m_end;
		m_locked_queue.Init(a_arena, RING_BUFFER_QUEUE_ELEMENT_COUNT);
	}

	UploadBuffer AllocateUploadMemory(const size_t a_byte_amount, const uint64_t a_fence_value)
	{
		BB_ASSERT(a_byte_amount < GetUploadAllocatorCapacity(), "trying to upload more memory then the ringbuffer size");
		OSAcquireSRWLockWrite(&m_lock);

		void* begin = m_write_at;
		void* end = Pointer::Add(m_write_at, a_byte_amount);

		bool must_free_memory = end > m_free_until ? true : false;

		// if we go over the end, but not over the readpointer then recalculate
		if (end > m_end)
		{
			begin = m_begin;
			end = Pointer::Add(m_begin, a_byte_amount);
			// is free_until larger then end? if yes then we can allocate without waiting
			must_free_memory = end > m_free_until ? true : false;
		}

		if (m_locked_queue.IsFull())
			must_free_memory = true;

		if (must_free_memory)
		{
			const uint64_t fence_value = Vulkan::GetCurrentFenceValue(m_fence);

			while (const UploadRingAllocator::LockedRegions* locked_region = m_locked_queue.Peek())
			{
				if (locked_region->fence_value <= fence_value)
				{
					m_free_until = locked_region->memory_end;
					m_locked_queue.DeQueue();
				}
				else
					break;
			}
		}

		UploadRingAllocator::LockedRegions locked_region;
		locked_region.memory_end = end;
		locked_region.fence_value = a_fence_value;
		m_locked_queue.EnQueue(locked_region);
		m_write_at = end;

		OSReleaseSRWLockWrite(&m_lock);

		UploadBuffer upload_buffer;
		upload_buffer.buffer = m_buffer;
		upload_buffer.begin = begin;
		upload_buffer.end = end;
		upload_buffer.base_offset = reinterpret_cast<size_t>(Pointer::Subtract(begin, reinterpret_cast<size_t>(m_begin)));
		return upload_buffer;
	}

	size_t GetUploadAllocatorCapacity() const
	{
		return reinterpret_cast<size_t>(m_end) - reinterpret_cast<size_t>(m_begin);
	}

private:
	struct LockedRegions
	{
		void* memory_end;
		uint64_t fence_value;
	};

	BBRWLock m_lock;
	RFence m_fence;
	GPUBuffer m_buffer;
	
	void* m_begin;
	void* m_free_until;
	void* m_write_at;
	void* m_end;
	Queue<LockedRegions> m_locked_queue;
};

//THREAD SAFE: TRUE
class BB::RenderQueue
{
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

	void ExecuteCommands(const RCommandList* const a_lists, const uint32_t a_list_count, const RFence* const a_signal_fences, const uint64_t* const a_signal_values, const uint32_t a_signal_count, const RFence* const a_wait_fences, const uint64_t* const a_wait_values, const uint32_t a_wait_count)
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
		++m_fence.next_fence_value;
		OSReleaseSRWLockWrite(&m_lock);
	}

	PRESENT_IMAGE_RESULT ExecutePresentCommands(RCommandList* const a_lists, const uint32_t a_list_count, const RFence* const a_signal_fences, const uint64_t* const a_signal_values, const uint32_t a_signal_count, const RFence* const a_wait_fences, const uint64_t* const a_wait_values, const uint32_t a_wait_count, const uint32_t a_backbuffer_index)
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
		++m_fence.next_fence_value;
		OSReleaseSRWLockWrite(&m_lock);
		return result;
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
	GPUBufferView vertex_buffer;
	GPUBufferView index_buffer;
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
	const char* shader_path;
	ShaderObjectCreateInfo create_info;
#endif // _ENABLE_REBUILD_SHADERS
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

union Light
{
	PointLight point_light;
};

constexpr uint32_t BACK_BUFFER_MAX = 3;

struct Scene3D
{
	struct Frame
	{
		GPUBuffer per_frame_buffer;
		size_t per_frame_buffer_size;

		struct PerFrameBufferPart
		{
			uint32_t offset;
			uint32_t size;
		};
		PerFrameBufferPart scene_buffer;
		PerFrameBufferPart transform_buffer;
		PerFrameBufferPart light_buffer;
		DescriptorAllocation desc_alloc;
		uint64_t fence_value;
	};

	Frame* frames;
	Scene3DInfo scene_info;

	RImage depth_image;
	RImageView depth_image_view;
	uint2 previous_draw_area;

	uint32_t draw_list_count;
	uint32_t draw_list_max;
	DrawList draw_list_data;

	const char* scene_name;

	//we have different scenes but the light handles are the same. Meaning we can accidently mess this up. Maybe make this a freelist instead of a slotmap.
	StaticSlotmap<PointLight, LightHandle> light_container;
};

struct RenderInterface_inst
{
	RenderInterface_inst(MemoryArena& a_arena)
		: graphics_queue(a_arena, QUEUE_TYPE::GRAPHICS, "graphics queue", 32, 32),
		  transfer_queue(a_arena, QUEUE_TYPE::TRANSFER, "transfer queue", 8, 8)
	{}

	RenderIO render_io;

	struct Frame
	{
		RTexture render_target;
		uint64_t graphics_queue_fence_value;
	};
	Frame* frames;
	bool debug;

	ShaderCompiler shader_compiler;

	// transfer queue should always use this
	UploadRingAllocator asset_upload_allocator;
	RFence asset_upload_fence;
	volatile uint64_t asset_upload_next_fence_value;
	BBRWLock asset_upload_value_lock;		// yes, a lock for a single variable

	UploadRingAllocator frame_upload_allocator;
	RenderQueue graphics_queue;
	RenderQueue transfer_queue;

	GPUTextureManager texture_manager;

	RTexture white;
	RTexture black;

	RDescriptorLayout static_sampler_descriptor_set;
	RDescriptorLayout global_descriptor_set;
	DescriptorAllocation global_descriptor_allocation;

	RDescriptorLayout scene3d_descriptor_layout;

	struct VertexBuffer
	{
		GPUBuffer buffer;
		uint32_t size;
		uint32_t used;
	} vertex_buffer;
	struct CPUVertexBuffer
	{
		GPUBuffer buffer;
		uint32_t size;
		uint32_t used;
		void* start_mapped;
	} cpu_vertex_buffer;

	struct IndexBuffer
	{
		GPUBuffer buffer;
		uint32_t size;
		uint32_t used;
	} index_buffer;
	struct CPUIndexBuffer
	{
		GPUBuffer buffer;
		uint32_t size;
		uint32_t used;
		void* start_mapped;
	} cpu_index_buffer;

	StaticSlotmap<Mesh, MeshHandle> mesh_map{};
	StaticSlotmap<ShaderEffect, ShaderEffectHandle> shader_effect_map{};
	StaticSlotmap<Material, MaterialHandle> material_map{};

	GPUDeviceInfo gpu_device;
};

static RenderInterface_inst* s_render_inst;

namespace IMGUI_IMPL
{
	constexpr size_t INITIAL_VERTEX_SIZE = sizeof(Vertex2D) * 128;
	constexpr size_t INITIAL_INDEX_SIZE = sizeof(uint32_t) * 256;

	struct ImRenderBuffer
	{
		WriteableGPUBufferView vertex_buffer;
		WriteableGPUBufferView index_buffer;
	};

	constexpr SHADER_STAGE IMGUI_SHADER_STAGES[2]{ SHADER_STAGE::VERTEX, SHADER_STAGE::FRAGMENT_PIXEL };

	struct ImRenderData
	{
		// 0 = VERTEX, 1 = FRAGMENT
		ShaderEffectHandle		shader_effects[2];	//16
		RTexture				font_image;         //20

		// Render buffers for main window
		uint32_t frame_index;                       //24
		ImRenderBuffer* frame_buffers;				//32

		//we just fetch em directly
		const ShaderEffect* vertex;
		const ShaderEffect* fragment;
	};

	inline static ImRenderData* ImGetRenderData()
	{
		return ImGui::GetCurrentContext() ? reinterpret_cast<ImRenderData*>(ImGui::GetIO().BackendRendererUserData) : nullptr;
	}

	inline static void ImSetRenderState(const ImDrawData& a_draw_data, const RCommandList a_cmd_list, const uint32_t a_vert_pos)
	{
		ImRenderData* bd = ImGetRenderData();

		{
			const ShaderObject shader_objects[2]{ bd->vertex->shader_object, bd->fragment->shader_object };
			Vulkan::BindShaders(a_cmd_list, _countof(IMGUI_SHADER_STAGES), IMGUI_SHADER_STAGES, shader_objects);
		}

		// Setup scale and translation:
		// Our visible imgui space lies from draw_data->DisplayPps (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayPos is (0,0) for single viewport apps.
		{
			ShaderIndices2D shader_indices;
			shader_indices.vertex_buffer_offset = a_vert_pos;
			shader_indices.albedo_texture = bd->font_image.handle;
			shader_indices.rect_scale.x = 2.0f / a_draw_data.DisplaySize.x;
			shader_indices.rect_scale.y = 2.0f / a_draw_data.DisplaySize.y;
			shader_indices.translate.x = -1.0f - a_draw_data.DisplayPos.x * shader_indices.rect_scale.x;
			shader_indices.translate.y = -1.0f - a_draw_data.DisplayPos.y * shader_indices.rect_scale.y;

			Vulkan::SetPushConstants(a_cmd_list, bd->vertex->pipeline_layout, 0, sizeof(shader_indices), &shader_indices);
		}
	}

	inline static void ImGrowFrameBufferGPUBuffers(ImRenderBuffer& a_rb, const size_t a_new_vertex_size, const size_t a_new_index_size)
	{
		// free I guess, lol can't do that now XDDD

		a_rb.vertex_buffer = AllocateFromWritableVertexBuffer(a_new_vertex_size);
		a_rb.index_buffer = AllocateFromWritableIndexBuffer(a_new_index_size);
	}


	static void ImRenderFrame(const RCommandList a_cmd_list, const RImageView render_target_view, const bool a_clear_image)
	{
		const ImDrawData& draw_data = *ImGui::GetDrawData();
		// Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
		int fb_width = static_cast<int>(draw_data.DisplaySize.x * draw_data.FramebufferScale.x);
		int fb_height = static_cast<int>(draw_data.DisplaySize.y * draw_data.FramebufferScale.y);
		if (fb_width <= 0 || fb_height <= 0)
			return;

		ImRenderData* bd = ImGetRenderData();
		const RenderIO& render_io = GetRenderIO();

		BB_ASSERT(bd->frame_index < render_io.frame_count, "Frame index is higher then the framebuffer amount! Forgot to resize the imgui window info.");
		bd->frame_index = (bd->frame_index + 1) % render_io.frame_count;
		ImRenderBuffer& rb = bd->frame_buffers[bd->frame_index];

		if (draw_data.TotalVtxCount > 0)
		{
			// Create or resize the vertex/index buffers
			const size_t vertex_size = static_cast<size_t>(draw_data.TotalVtxCount) * sizeof(ImDrawVert);
			const size_t index_size = static_cast<size_t>(draw_data.TotalIdxCount) * sizeof(ImDrawIdx);
			if (rb.vertex_buffer.size < vertex_size || rb.index_buffer.size < index_size)
				ImGrowFrameBufferGPUBuffers(rb, Max(rb.vertex_buffer.size * 2, vertex_size), Max(rb.index_buffer.size * 2, index_size));


			BB_STATIC_ASSERT(sizeof(Vertex2D) == sizeof(ImDrawVert), "Vertex2D size is not the same as ImDrawVert");
			BB_STATIC_ASSERT(IM_OFFSETOF(Vertex2D, position) == IM_OFFSETOF(ImDrawVert, pos), "Vertex2D does not have the same offset for the position variable as ImDrawVert");
			BB_STATIC_ASSERT(IM_OFFSETOF(Vertex2D, uv) == IM_OFFSETOF(ImDrawVert, uv), "Vertex2D does not have the same offset for the uv variable as ImDrawVert");
			BB_STATIC_ASSERT(IM_OFFSETOF(Vertex2D, color) == IM_OFFSETOF(ImDrawVert, col), "Vertex2D does not have the same offset for the color variable as ImDrawVert");

			// Upload vertex/index data into a single contiguous GPU buffer
			ImDrawVert* vtx_dst = reinterpret_cast<ImDrawVert*>(rb.vertex_buffer.mapped);
			ImDrawIdx* idx_dst = reinterpret_cast<ImDrawIdx*>(rb.index_buffer.mapped);

			for (int n = 0; n < draw_data.CmdListsCount; n++)
			{
				const ImDrawList* cmd_list = draw_data.CmdLists[n];
				Memory::Copy(vtx_dst, cmd_list->VtxBuffer.Data, static_cast<size_t>(cmd_list->VtxBuffer.Size));
				Memory::Copy(idx_dst, cmd_list->IdxBuffer.Data, static_cast<size_t>(cmd_list->IdxBuffer.Size));
				vtx_dst += cmd_list->VtxBuffer.Size;
				idx_dst += cmd_list->IdxBuffer.Size;
			}
		}

		StartRenderingInfo imgui_pass_start{};
		imgui_pass_start.viewport_size.x = render_io.screen_width;
		imgui_pass_start.viewport_size.y = render_io.screen_height;
		imgui_pass_start.scissor_extent = imgui_pass_start.viewport_size;
		imgui_pass_start.scissor_offset = {};
		imgui_pass_start.depth_view = RImageView(BB_INVALID_HANDLE_64);
		imgui_pass_start.load_color = !a_clear_image;
		imgui_pass_start.store_color = true;
		imgui_pass_start.layout = IMAGE_LAYOUT::GENERAL;
		Vulkan::StartRenderPass(a_cmd_list, imgui_pass_start, render_target_view);

		// Setup desired CrossRenderer state
		ImSetRenderState(draw_data, a_cmd_list, 0);

		// Will project scissor/clipping rectangles into framebuffer space
		const ImVec2 clip_off = draw_data.DisplayPos;    // (0,0) unless using multi-viewports
		const ImVec2 clip_scale = draw_data.FramebufferScale; // (1,1) unless using retina display which are often (2,2)

		// Because we merged all buffers into a single one, we maintain our own offset into them
		uint32_t global_idx_offset = 0;
		uint32_t vertex_offset = static_cast<uint32_t>(rb.vertex_buffer.offset);

		Vulkan::BindIndexBuffer(a_cmd_list, rb.index_buffer.buffer, rb.index_buffer.offset);

		ImTextureID last_texture = bd->font_image.handle;
		for (int n = 0; n < draw_data.CmdListsCount; n++)
		{
			const ImDrawList* cmd_list = draw_data.CmdLists[n];
			Vulkan::SetPushConstants(a_cmd_list, bd->vertex->pipeline_layout, IM_OFFSETOF(ShaderIndices2D, vertex_buffer_offset), sizeof(vertex_offset), &vertex_offset);

			for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
			{
				const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
				if (pcmd->UserCallback != nullptr)
				{
					// User callback, registered via ImDrawList::AddCallback()
					// (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
					if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
						ImSetRenderState(draw_data, a_cmd_list, vertex_offset);
					else
						pcmd->UserCallback(cmd_list, pcmd);
				}
				else
				{
					// Project scissor/clipping rectangles into framebuffer space
					ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
					ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);

					// Clamp to viewport as vkCmdSetScissor() won't accept values that are off bounds
					if (clip_min.x < 0.0f) { clip_min.x = 0.0f; }
					if (clip_min.y < 0.0f) { clip_min.y = 0.0f; }
					if (clip_max.x > static_cast<float>(fb_width)) { clip_max.x = static_cast<float>(fb_width); }
					if (clip_max.y > static_cast<float>(fb_height)) { clip_max.y = static_cast<float>(fb_height); }
					if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
						continue;

					const ImTextureID new_text = pcmd->TextureId;
					if (new_text != last_texture)
					{
						Vulkan::SetPushConstants(a_cmd_list, bd->vertex->pipeline_layout, IM_OFFSETOF(ShaderIndices2D, albedo_texture), sizeof(new_text), &new_text);
						last_texture = new_text;
					}
					// Apply scissor/clipping rectangle
					ScissorInfo scissor;
					scissor.offset.x = static_cast<int32_t>(clip_min.x);
					scissor.offset.y = static_cast<int32_t>(clip_min.y);
					scissor.extent.x = static_cast<uint32_t>(clip_max.x);
					scissor.extent.y = static_cast<uint32_t>(clip_max.y);
					Vulkan::SetScissor(a_cmd_list, scissor);

					// Draw
					const uint32_t index_offset = pcmd->IdxOffset + global_idx_offset;
					Vulkan::DrawIndexed(a_cmd_list, pcmd->ElemCount, 1, index_offset, 0, 0);
				}
			}
			vertex_offset += static_cast<uint32_t>(cmd_list->VtxBuffer.size_in_bytes());
			global_idx_offset += static_cast<uint32_t>(cmd_list->IdxBuffer.Size);
		}

		// Since we dynamically set our scissor lets set it back to the full viewport. 
		// This might be bad to do since this can leak into different system's code. 
		ScissorInfo scissor{};
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		scissor.extent.x = static_cast<uint32_t>(fb_width);
		scissor.extent.y = static_cast<uint32_t>(fb_height);
		Vulkan::SetScissor(a_cmd_list, scissor);

		Vulkan::EndRenderPass(a_cmd_list);
	}

	static bool ImInit(MemoryArena& a_arena, const RCommandList a_cmd_list, const uint64_t a_asset_fence_value)
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsClassic();

		BB_STATIC_ASSERT(sizeof(ImDrawIdx) == sizeof(uint32_t), "Index size is not 32 bit, it must be 32 bit.");

		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
		IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");

		// Setup backend capabilities flags
		ImRenderData* bd = ArenaAllocType(a_arena, ImRenderData);
		io.BackendRendererName = "imgui-modern-vulkan";
		io.BackendRendererUserData = reinterpret_cast<void*>(bd);

		MemoryArenaScope(a_arena)
		{
			CreateShaderEffectInfo shaders[2];
			shaders[0].name = "imgui vertex shader";
			shaders[0].shader_path = "../resources/shaders/hlsl/Imgui.hlsl";
			shaders[0].shader_entry = "VertexMain";
			shaders[0].stage = SHADER_STAGE::VERTEX;
			shaders[0].next_stages = static_cast<SHADER_STAGE_FLAGS>(SHADER_STAGE::FRAGMENT_PIXEL);
			shaders[0].push_constant_space = sizeof(ShaderIndices2D);
			shaders[0].pass_type = RENDER_PASS_TYPE::STANDARD_3D;

			shaders[1].name = "imgui Fragment shader";
			shaders[1].shader_path = "../resources/shaders/hlsl/Imgui.hlsl";
			shaders[1].shader_entry = "FragmentMain";
			shaders[1].stage = SHADER_STAGE::FRAGMENT_PIXEL;
			shaders[1].next_stages = static_cast<SHADER_STAGE_FLAGS>(SHADER_STAGE::NONE);
			shaders[1].push_constant_space = sizeof(ShaderIndices2D);
			shaders[1].pass_type = RENDER_PASS_TYPE::STANDARD_3D;

			BB_ASSERT(CreateShaderEffect(a_arena, Slice(shaders, _countof(shaders)), bd->shader_effects),
				"Failed to create imgui shaders");

			bd->vertex = &s_render_inst->shader_effect_map.find(bd->shader_effects[0]);
			bd->fragment = &s_render_inst->shader_effect_map.find(bd->shader_effects[1]);
		}

		//create framebuffers.
		{
			const RenderIO render_io = GetRenderIO();
			bd->frame_index = 0;
			bd->frame_buffers = ArenaAllocArr(a_arena, ImRenderBuffer,  render_io.frame_count);

			for (size_t i = 0; i < render_io.frame_count; i++)
			{
				//I love C++
				new (&bd->frame_buffers[i])(ImRenderBuffer);
				ImRenderBuffer& rb = bd->frame_buffers[i];

				rb.vertex_buffer = AllocateFromWritableVertexBuffer(INITIAL_VERTEX_SIZE);
				rb.index_buffer = AllocateFromWritableIndexBuffer(INITIAL_INDEX_SIZE);
			}
		}

		unsigned char* pixels;
		int width, height;
		io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

		UploadTextureInfo font_info{};
		font_info.name = "imgui font";
		font_info.width = static_cast<uint32_t>(width);
		font_info.height = static_cast<uint32_t>(height);
		font_info.format = IMAGE_FORMAT::RGBA8_UNORM;
		font_info.pixels = pixels;
		bd->font_image = UploadTexture(a_cmd_list, font_info, a_asset_fence_value);

		io.Fonts->SetTexID(bd->font_image.handle);

		return bd->font_image.IsValid();
	}

	inline static void ImShutdown()
	{
		ImRenderData* bd = ImGetRenderData();
		BB_ASSERT(bd != nullptr, "No renderer backend to shutdown, or already shutdown?");
		ImGuiIO& io = ImGui::GetIO();

		//delete my things here.
		FreeTexture(bd->font_image);
		bd->font_image = RTexture(BB_INVALID_HANDLE_32);

		FreeShaderEffect(bd->shader_effects[0]);
		FreeShaderEffect(bd->shader_effects[1]);

		io.BackendRendererName = nullptr;
		io.BackendRendererUserData = nullptr;

		ImGui::DestroyContext();
	}

	inline static void ImNewFrame()
	{
		ImGuiIO& io = ImGui::GetIO();

		io.DisplaySize = ImVec2(
			static_cast<float>(s_render_inst->render_io.screen_width), 
			static_cast<float>(s_render_inst->render_io.screen_height));
	}
} // IMGUI_IMPL

static void DisplayGPUInfo()
{
	if (ImGui::CollapsingHeader("gpu info"))
	{
		ImGui::Indent();
		const GPUDeviceInfo& gpu = s_render_inst->gpu_device;
		ImGui::TextUnformatted(gpu.name);
		if (ImGui::CollapsingHeader("memory heaps"))
		{
			ImGui::Indent();
			for (uint32_t i = 0; i < static_cast<uint32_t>(gpu.memory_heaps.size()); i++)
			{
				ImGui::Text("heap: %u", gpu.memory_heaps[i].heap_num);
				ImGui::Text("heap size: %lld", gpu.memory_heaps[i].heap_size);
				ImGui::Text("heap is device local %d", gpu.memory_heaps[i].heap_device_local);
			}
			ImGui::Unindent();
		}

		if (ImGui::CollapsingHeader("queue families"))
		{
			ImGui::Indent();
			for (size_t i = 0; i < gpu.queue_families.size(); i++)
			{
				ImGui::Text("family index: %u", gpu.queue_families[i].queue_family_index);
				ImGui::Text("queue count: %u", gpu.queue_families[i].queue_count);
				ImGui::Text("support compute: %d", gpu.queue_families[i].support_compute);
				ImGui::Text("support graphics: %d", gpu.queue_families[i].support_graphics);
				ImGui::Text("support transfer: %d", gpu.queue_families[i].support_transfer);
			}
			ImGui::Unindent();
		}
		ImGui::Unindent();
	}
}

static void ImguiDisplayRenderer()
{
	if (ImGui::Begin("Renderer"))
	{
		DisplayGPUInfo();
		s_render_inst->texture_manager.DisplayTextureListImgui();

		if (ImGui::CollapsingHeader("Reload imgui shaders"))
		{
			ImGui::Indent();
			IMGUI_IMPL::ImRenderData* render_data = IMGUI_IMPL::ImGetRenderData();
			if (ImGui::Button("Vertex"))
			{
				ReloadShaderEffect(render_data->shader_effects[0]);
			}
			if (ImGui::Button("Fragment"))
			{
				ReloadShaderEffect(render_data->shader_effects[1]);
			}
			ImGui::Unindent();
		}
	}
	ImGui::End();
}

GPUBufferView BB::AllocateFromVertexBuffer(const size_t a_size_in_bytes)
{
	GPUBufferView view;
	view.buffer = s_render_inst->vertex_buffer.buffer;
	view.size = a_size_in_bytes;
	view.offset = s_render_inst->vertex_buffer.used;

	s_render_inst->vertex_buffer.used += static_cast<uint32_t>(a_size_in_bytes);
	BB_ASSERT(s_render_inst->vertex_buffer.size > s_render_inst->vertex_buffer.used, "out of vertex buffer space!");

	return view;
}

GPUBufferView BB::AllocateFromIndexBuffer(const size_t a_size_in_bytes)
{
	GPUBufferView view;
	view.buffer = s_render_inst->index_buffer.buffer;
	view.size = a_size_in_bytes;
	view.offset = s_render_inst->index_buffer.used;

	s_render_inst->index_buffer.used += static_cast<uint32_t>(a_size_in_bytes);
	BB_ASSERT(s_render_inst->index_buffer.size > s_render_inst->index_buffer.used, "out of index buffer space!");

	return view;
}

WriteableGPUBufferView BB::AllocateFromWritableVertexBuffer(const size_t a_size_in_bytes)
{
	WriteableGPUBufferView view;
	view.buffer = s_render_inst->cpu_vertex_buffer.buffer;
	view.size = a_size_in_bytes;
	view.offset = s_render_inst->cpu_vertex_buffer.used;
	view.mapped = Pointer::Add(s_render_inst->cpu_vertex_buffer.start_mapped, s_render_inst->cpu_vertex_buffer.used);

	s_render_inst->cpu_vertex_buffer.used += static_cast<uint32_t>(a_size_in_bytes);

	return view;
}

WriteableGPUBufferView BB::AllocateFromWritableIndexBuffer(const size_t a_size_in_bytes)
{
	WriteableGPUBufferView view;
	view.buffer = s_render_inst->cpu_index_buffer.buffer;
	view.size = a_size_in_bytes;
	view.offset = s_render_inst->cpu_index_buffer.used;
	view.mapped = Pointer::Add(s_render_inst->cpu_index_buffer.start_mapped, s_render_inst->cpu_index_buffer.used);

	s_render_inst->cpu_index_buffer.used += static_cast<uint32_t>(a_size_in_bytes);

	return view;
}

void GPUTextureManager::Init(MemoryArena& a_arena, const RCommandList a_list, const uint64_t a_upload_fence_value)
{
	m_graphics_texture_transitions.Init(a_arena, MAX_TEXTURES / 4);

	{	//special debug image that gets placed on ALL empty or fried texture slots.
		ImageCreateInfo image_info;
		image_info.name = "debug purple";
		image_info.width = 1;
		image_info.height = 1;
		image_info.depth = 1;
		image_info.array_layers = 1;
		image_info.mip_levels = 1;
		image_info.type = IMAGE_TYPE::TYPE_2D;
		image_info.tiling = IMAGE_TILING::OPTIMAL;
		image_info.format = IMAGE_FORMAT::RGBA8_SRGB;
		image_info.usage = IMAGE_USAGE::TEXTURE;
		m_debug_texture.image = Vulkan::CreateImage(image_info);

		ImageViewCreateInfo image_view_info;
		image_view_info.image = m_debug_texture.image;
		image_view_info.name = image_info.name;
		image_view_info.array_layers = 1;
		image_view_info.mip_levels = 1;
		image_view_info.type = image_info.type;
		image_view_info.format = image_info.format;
		m_debug_texture.view = Vulkan::CreateViewImage(image_view_info);

		m_debug_texture.width = 1;
		m_debug_texture.height = 1;
		m_debug_texture.format = IMAGE_FORMAT::RGBA8_SRGB;
		{
			PipelineBarrierImageInfo image_write_transition;
			image_write_transition.src_mask = BARRIER_ACCESS_MASK::NONE;
			image_write_transition.dst_mask = BARRIER_ACCESS_MASK::TRANSFER_WRITE;
			image_write_transition.image = m_debug_texture.image;
			image_write_transition.old_layout = IMAGE_LAYOUT::UNDEFINED;
			image_write_transition.new_layout = IMAGE_LAYOUT::TRANSFER_DST;
			image_write_transition.src_queue = QUEUE_TRANSITION::NO_TRANSITION;
			image_write_transition.dst_queue = QUEUE_TRANSITION::NO_TRANSITION;
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
		const uint32_t debug_purple = (209u << 24u) | (106u << 16u) | (255u << 8u) | (255u << 0u);

		//now upload the image.
		UploadBuffer upload_buffer = s_render_inst->asset_upload_allocator.AllocateUploadMemory(sizeof(debug_purple), a_upload_fence_value);
		upload_buffer.SafeMemcpy(0, &debug_purple, sizeof(debug_purple));

		RenderCopyBufferToImageInfo buffer_to_image;
		buffer_to_image.src_buffer = upload_buffer.buffer;
		buffer_to_image.src_offset = static_cast<uint32_t>(upload_buffer.base_offset);

		buffer_to_image.dst_image = m_debug_texture.image;
		buffer_to_image.dst_image_info.size_x = 1;
		buffer_to_image.dst_image_info.size_y = 1;
		buffer_to_image.dst_image_info.size_z = 1;
		buffer_to_image.dst_image_info.offset_x = 0;
		buffer_to_image.dst_image_info.offset_y = 0;
		buffer_to_image.dst_image_info.offset_z = 0;
		buffer_to_image.dst_image_info.layout = IMAGE_LAYOUT::TRANSFER_DST;
		buffer_to_image.dst_image_info.mip_level = 0;
		buffer_to_image.dst_image_info.layer_count = 1;
		buffer_to_image.dst_image_info.base_array_layer = 0;

		Vulkan::CopyBufferToImage(a_list, buffer_to_image);

		{
			PipelineBarrierImageInfo image_shader_transition;
			image_shader_transition.src_mask = BARRIER_ACCESS_MASK::TRANSFER_WRITE;
			image_shader_transition.dst_mask = BARRIER_ACCESS_MASK::SHADER_READ;
			image_shader_transition.image = m_debug_texture.image;
			image_shader_transition.old_layout = IMAGE_LAYOUT::TRANSFER_DST;
			image_shader_transition.new_layout = IMAGE_LAYOUT::SHADER_READ_ONLY;
			image_shader_transition.src_queue = QUEUE_TRANSITION::NO_TRANSITION;
			image_shader_transition.dst_queue = QUEUE_TRANSITION::NO_TRANSITION;
			image_shader_transition.layer_count = 1;
			image_shader_transition.level_count = 1;
			image_shader_transition.base_array_layer = 0;
			image_shader_transition.base_mip_level = 0;
			image_shader_transition.src_stage = BARRIER_PIPELINE_STAGE::TRANSFER;
			image_shader_transition.dst_stage = BARRIER_PIPELINE_STAGE::FRAGMENT_SHADER;

			m_graphics_texture_transitions.emplace_back(image_shader_transition);

			m_debug_texture.current_layout = image_shader_transition.new_layout;
		}

		WriteDescriptorData write_desc[MAX_TEXTURES]{};

		for (uint32_t i = 0; i < MAX_TEXTURES; i++)
		{
			write_desc[i].binding = 2;
			write_desc[i].descriptor_index = i; //handle is also the descriptor index
			write_desc[i].type = DESCRIPTOR_TYPE::IMAGE;
			write_desc[i].image_view.view = m_debug_texture.view;
			write_desc[i].image_view.layout = IMAGE_LAYOUT::SHADER_READ_ONLY;
		}

		WriteDescriptorInfos image_write_infos;
		image_write_infos.allocation = s_render_inst->global_descriptor_allocation;
		image_write_infos.descriptor_layout = s_render_inst->global_descriptor_set;
		image_write_infos.data = Slice(write_desc, _countof(write_desc));

		Vulkan::WriteDescriptors(image_write_infos);
	}

	m_lock = OSCreateRWLock();
	//texture 0 is always the debug texture.
	m_textures[0].texture_info = m_debug_texture;
	m_textures[0].name = DEBUG_TEXTURE_NAME;
	m_textures[0].next_free = UINT32_MAX;

	m_next_free = 1;

	for (uint32_t i = 1; i < MAX_TEXTURES - 1; i++)
	{
		m_textures[i].texture_info = m_debug_texture;
		m_textures[i].name = DEBUG_TEXTURE_NAME;
		m_textures[i].next_free = i + 1;
	}

	m_textures[MAX_TEXTURES - 1].texture_info = m_debug_texture;
	m_textures[MAX_TEXTURES - 1].next_free = UINT32_MAX;
}

const RTexture GPUTextureManager::UploadTexture(const RCommandList a_list, const UploadTextureInfo& a_upload_info, const uint64_t a_upload_fence_value)
{
	OSAcquireSRWLockWrite(&m_lock);
	const RTexture texture_slot = RTexture(m_next_free);
	TextureSlot& slot = m_textures[texture_slot.handle];
	m_next_free = slot.next_free;
	OSReleaseSRWLockWrite(&m_lock);

	slot.name = a_upload_info.name;

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
		image_info.format = a_upload_info.format;
		image_info.usage = IMAGE_USAGE::TEXTURE;
		switch (image_info.format)
		{
		case IMAGE_FORMAT::RGBA16_UNORM:
		case IMAGE_FORMAT::RGBA16_SFLOAT:
			byte_per_pixel = 8;
			break;
		case IMAGE_FORMAT::RGBA8_SRGB:
		case IMAGE_FORMAT::RGBA8_UNORM:
			byte_per_pixel = 4;
			break;
		case IMAGE_FORMAT::A8_UNORM:
			byte_per_pixel = 1;
			break;
		default:
			BB_ASSERT(false, "Unsupported bit_count for upload image");
			image_info.format = IMAGE_FORMAT::RGBA8_SRGB;
			byte_per_pixel = 4;
			break;
		}

		slot.texture_info.image = Vulkan::CreateImage(image_info);

		ImageViewCreateInfo image_view_info;
		image_view_info.image = slot.texture_info.image;
		image_view_info.name = a_upload_info.name;
		image_view_info.array_layers = 1;
		image_view_info.mip_levels = 1;
		image_view_info.type = image_info.type;
		image_view_info.format = image_info.format;
		slot.texture_info.view = Vulkan::CreateViewImage(image_view_info);

		PipelineBarrierImageInfo image_write_transition;
		image_write_transition.src_mask = BARRIER_ACCESS_MASK::NONE;
		image_write_transition.dst_mask = BARRIER_ACCESS_MASK::TRANSFER_WRITE;
		image_write_transition.image = slot.texture_info.image;
		image_write_transition.old_layout = IMAGE_LAYOUT::UNDEFINED;
		image_write_transition.new_layout = IMAGE_LAYOUT::TRANSFER_DST;
		image_write_transition.src_queue = QUEUE_TRANSITION::NO_TRANSITION;
		image_write_transition.dst_queue = QUEUE_TRANSITION::NO_TRANSITION;
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

	slot.texture_info.width = a_upload_info.width;
	slot.texture_info.height = a_upload_info.height;
	slot.texture_info.format = a_upload_info.format;

	// if we have no data to upload then just return the empty image.
	if (a_upload_info.pixels == nullptr)
	{
		WriteDescriptorData write_desc{};
		write_desc.binding = GLOBAL_BINDLESS_TEXTURES_BINDING;
		write_desc.descriptor_index = texture_slot.handle; // handle is also the descriptor index
		write_desc.type = DESCRIPTOR_TYPE::IMAGE;
		write_desc.image_view.view = slot.texture_info.view;
		write_desc.image_view.layout = IMAGE_LAYOUT::TRANSFER_DST;

		WriteDescriptorInfos image_write_infos;
		image_write_infos.allocation = s_render_inst->global_descriptor_allocation;
		image_write_infos.descriptor_layout = s_render_inst->global_descriptor_set;
		image_write_infos.data = Slice(&write_desc, 1);

		Vulkan::WriteDescriptors(image_write_infos);

		slot.texture_info.current_layout = IMAGE_LAYOUT::TRANSFER_DST;

		return texture_slot;
	}
	else
	{
		WriteDescriptorData write_desc{};
		write_desc.binding = GLOBAL_BINDLESS_TEXTURES_BINDING;
		write_desc.descriptor_index = texture_slot.handle; // handle is also the descriptor index
		write_desc.type = DESCRIPTOR_TYPE::IMAGE;
		write_desc.image_view.view = slot.texture_info.view;
		write_desc.image_view.layout = IMAGE_LAYOUT::SHADER_READ_ONLY;

		WriteDescriptorInfos image_write_infos;
		image_write_infos.allocation = s_render_inst->global_descriptor_allocation;
		image_write_infos.descriptor_layout = s_render_inst->global_descriptor_set;
		image_write_infos.data = Slice(&write_desc, 1);

		Vulkan::WriteDescriptors(image_write_infos);

		slot.texture_info.current_layout = IMAGE_LAYOUT::SHADER_READ_ONLY;
	}


	//now upload the image.
	UploadBuffer upload_buffer = s_render_inst->asset_upload_allocator.AllocateUploadMemory(byte_per_pixel * a_upload_info.width * a_upload_info.height, a_upload_fence_value);
	upload_buffer.SafeMemcpy(0, a_upload_info.pixels, byte_per_pixel * a_upload_info.width * a_upload_info.height);

	RenderCopyBufferToImageInfo buffer_to_image;
	buffer_to_image.src_buffer = upload_buffer.buffer;
	buffer_to_image.src_offset = static_cast<uint32_t>(upload_buffer.base_offset);

	buffer_to_image.dst_image = slot.texture_info.image;
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

	Vulkan::CopyBufferToImage(a_list, buffer_to_image);

	{
		PipelineBarrierImageInfo image_shader_transition;
		image_shader_transition.src_mask = BARRIER_ACCESS_MASK::TRANSFER_WRITE;
		image_shader_transition.dst_mask = BARRIER_ACCESS_MASK::SHADER_READ;
		image_shader_transition.image = slot.texture_info.image;
		image_shader_transition.old_layout = IMAGE_LAYOUT::TRANSFER_DST;
		image_shader_transition.new_layout = IMAGE_LAYOUT::SHADER_READ_ONLY;
		image_shader_transition.src_queue = QUEUE_TRANSITION::NO_TRANSITION;
		image_shader_transition.dst_queue = QUEUE_TRANSITION::NO_TRANSITION;
		image_shader_transition.layer_count = 1;
		image_shader_transition.level_count = 1;
		image_shader_transition.base_array_layer = 0;
		image_shader_transition.base_mip_level = 0;
		image_shader_transition.src_stage = BARRIER_PIPELINE_STAGE::TRANSFER;
		image_shader_transition.dst_stage = BARRIER_PIPELINE_STAGE::FRAGMENT_SHADER;
		m_graphics_texture_transitions.emplace_back(image_shader_transition);
	}

	return texture_slot;
}

const RTexture GPUTextureManager::CreateRenderTarget(const uint2 a_render_target_extent, const char* a_name)
{
	OSAcquireSRWLockWrite(&m_lock);
	const RTexture texture_slot = RTexture(m_next_free);
	TextureSlot& slot = m_textures[texture_slot.handle];
	m_next_free = slot.next_free;
	OSReleaseSRWLockWrite(&m_lock);

	ImageCreateInfo image_info;
	image_info.name = a_name;
	image_info.width = a_render_target_extent.x;
	image_info.height = a_render_target_extent.y;
	image_info.depth = 1;
	image_info.array_layers = 1;
	image_info.mip_levels = 1;

	image_info.type = IMAGE_TYPE::TYPE_2D;
	image_info.tiling = IMAGE_TILING::OPTIMAL;
	image_info.format = RENDER_TARGET_IMAGE_FORMAT;
	image_info.usage = IMAGE_USAGE::RENDER_TARGET;
	slot.texture_info.image = Vulkan::CreateImage(image_info);

	ImageViewCreateInfo image_view_info;
	image_view_info.image = slot.texture_info.image;
	image_view_info.name = a_name;
	image_view_info.array_layers = 1;
	image_view_info.mip_levels = 1;
	image_view_info.type = image_info.type;
	image_view_info.format = image_info.format;
	slot.texture_info.view = Vulkan::CreateViewImage(image_view_info);

	WriteDescriptorData write_desc{};
	write_desc.binding = GLOBAL_BINDLESS_TEXTURES_BINDING;
	write_desc.descriptor_index = texture_slot.handle; //handle is also the descriptor index
	write_desc.type = DESCRIPTOR_TYPE::IMAGE;
	write_desc.image_view.view = slot.texture_info.view;
	write_desc.image_view.layout = IMAGE_LAYOUT::SHADER_READ_ONLY;

	WriteDescriptorInfos image_write_infos;
	image_write_infos.allocation = s_render_inst->global_descriptor_allocation;
	image_write_infos.descriptor_layout = s_render_inst->global_descriptor_set;
	image_write_infos.data = Slice(&write_desc, 1);

	Vulkan::WriteDescriptors(image_write_infos);


	slot.name = a_name;
	slot.texture_info.current_layout = IMAGE_LAYOUT::SHADER_READ_ONLY;
	slot.texture_info.format = RENDER_TARGET_IMAGE_FORMAT;
	slot.texture_info.width = a_render_target_extent.x;
	slot.texture_info.height = a_render_target_extent.y;

	return texture_slot;
}

const RTexture GPUTextureManager::CreateUploadTarget(const uint2 a_extent, const char* a_name)
{
	OSAcquireSRWLockWrite(&m_lock);
	const RTexture texture_slot = RTexture(m_next_free);
	TextureSlot& slot = m_textures[texture_slot.handle];
	m_next_free = slot.next_free;
	OSReleaseSRWLockWrite(&m_lock);

	ImageCreateInfo image_info;
	image_info.name = a_name;
	image_info.width = a_extent.x;
	image_info.height = a_extent.y;
	image_info.depth = 1;
	image_info.array_layers = 1;
	image_info.mip_levels = 1;

	image_info.type = IMAGE_TYPE::TYPE_2D;
	image_info.tiling = IMAGE_TILING::OPTIMAL;
	image_info.format = RENDER_TARGET_IMAGE_FORMAT;
	image_info.usage = IMAGE_USAGE::SWAPCHAIN_COPY_IMG;

	slot.texture_info.image = Vulkan::CreateImage(image_info);

	ImageViewCreateInfo image_view_info;
	image_view_info.image = slot.texture_info.image;
	image_view_info.name = a_name;
	image_view_info.array_layers = 1;
	image_view_info.mip_levels = 1;
	image_view_info.type = image_info.type;
	image_view_info.format = image_info.format;
	slot.texture_info.view = Vulkan::CreateViewImage(image_view_info);

	WriteDescriptorData write_desc{};
	write_desc.binding = GLOBAL_BINDLESS_TEXTURES_BINDING;
	write_desc.descriptor_index = texture_slot.handle; //handle is also the descriptor index
	write_desc.type = DESCRIPTOR_TYPE::IMAGE;
	write_desc.image_view.view = slot.texture_info.view;
	write_desc.image_view.layout = IMAGE_LAYOUT::TRANSFER_SRC;

	WriteDescriptorInfos image_write_infos;
	image_write_infos.allocation = s_render_inst->global_descriptor_allocation;
	image_write_infos.descriptor_layout = s_render_inst->global_descriptor_set;
	image_write_infos.data = Slice(&write_desc, 1);

	Vulkan::WriteDescriptors(image_write_infos);
	return texture_slot;
}

const RTexture GPUTextureManager::CreateDepthImage(const uint2 a_depth_image_extent, const char* a_name)
{
	OSAcquireSRWLockWrite(&m_lock);
	const RTexture texture_slot = RTexture(m_next_free);
	TextureSlot& slot = m_textures[texture_slot.handle];
	m_next_free = slot.next_free;
	OSReleaseSRWLockWrite(&m_lock);

	ImageCreateInfo image_info;
	image_info.name = a_name;
	image_info.width = a_depth_image_extent.x;
	image_info.height = a_depth_image_extent.y;
	image_info.depth = 1;
	image_info.array_layers = 1;
	image_info.mip_levels = 1;

	image_info.type = IMAGE_TYPE::TYPE_2D;
	image_info.tiling = IMAGE_TILING::OPTIMAL;
	image_info.format = DEPTH_IMAGE_FORMAT;
	image_info.usage = IMAGE_USAGE::TEXTURE;

	slot.texture_info.image = Vulkan::CreateImage(image_info);

	ImageViewCreateInfo image_view_info;
	image_view_info.image = slot.texture_info.image;
	image_view_info.name = a_name;
	image_view_info.array_layers = 1;
	image_view_info.mip_levels = 1;
	image_view_info.type = image_info.type;
	image_view_info.format = image_info.format;
	slot.texture_info.view = Vulkan::CreateViewImage(image_view_info);

	WriteDescriptorData write_desc{};
	write_desc.binding = GLOBAL_BINDLESS_TEXTURES_BINDING;
	write_desc.descriptor_index = texture_slot.handle; //handle is also the descriptor index
	write_desc.type = DESCRIPTOR_TYPE::IMAGE;
	write_desc.image_view.view = slot.texture_info.view;
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
	TextureSlot& slot = m_textures[a_texture.handle];
	Vulkan::FreeImage(slot.texture_info.image);
	Vulkan::FreeViewImage(slot.texture_info.view);

	OSAcquireSRWLockWrite(&m_lock);
	slot.next_free = m_next_free;
	m_next_free = a_texture.handle;
	OSReleaseSRWLockWrite(&m_lock);

	slot.name = DEBUG_TEXTURE_NAME;
	slot.texture_info = m_debug_texture;
}

const RenderIO& BB::GetRenderIO()
{
	return s_render_inst->render_io;
}

bool BB::InitializeRenderer(MemoryArena& a_arena, const RendererCreateInfo& a_render_create_info)
{
	Vulkan::InitializeVulkan(a_arena, a_render_create_info.app_name, a_render_create_info.engine_name, a_render_create_info.debug);
	s_render_inst = ArenaAllocType(a_arena, RenderInterface_inst)(a_arena);
	s_render_inst->render_io.frame_count = BACK_BUFFER_MAX;
	s_render_inst->render_io.frame_index = 0;

	Vulkan::CreateSwapchain(a_arena, a_render_create_info.window_handle, a_render_create_info.swapchain_width, a_render_create_info.swapchain_height, s_render_inst->render_io.frame_count);

	s_render_inst->render_io.window_handle = a_render_create_info.window_handle;
	s_render_inst->render_io.screen_width = a_render_create_info.swapchain_width;
	s_render_inst->render_io.screen_height = a_render_create_info.swapchain_height;
	s_render_inst->debug = a_render_create_info.debug;

	s_render_inst->mesh_map.Init(a_arena, 256);
	s_render_inst->shader_effect_map.Init(a_arena, 64);
	s_render_inst->material_map.Init(a_arena, 256);


	s_render_inst->frame_upload_allocator.Init(a_arena, a_render_create_info.frame_upload_buffer_size, s_render_inst->graphics_queue.GetFence().fence, "frame upload allocator");

	{	// do asset upload allocator here
		s_render_inst->asset_upload_value_lock = OSCreateRWLock();
		s_render_inst->asset_upload_fence = Vulkan::CreateFence(0, "asset upload fence");
		s_render_inst->asset_upload_next_fence_value = 1;
		s_render_inst->asset_upload_allocator.Init(a_arena, a_render_create_info.asset_upload_buffer_size, s_render_inst->asset_upload_fence, "asset upload buffer");

	}

	s_render_inst->shader_compiler = CreateShaderCompiler(a_arena);

	MemoryArenaScope(a_arena)
	{
		{	//static sampler descriptor set 0
			SamplerCreateInfo immutable_sampler{};
			immutable_sampler.name = "standard 3d sampler";
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
			DescriptorBindingInfo descriptor_bindings[3];
			descriptor_bindings[0].binding = GLOBAL_VERTEX_BUFFER_BINDING;
			descriptor_bindings[0].count = 1;
			descriptor_bindings[0].shader_stage = SHADER_STAGE::VERTEX;
			descriptor_bindings[0].type = DESCRIPTOR_TYPE::READONLY_BUFFER;

			descriptor_bindings[1].binding = GLOBAL_CPU_VERTEX_BUFFER_BINDING;
			descriptor_bindings[1].count = 1;
			descriptor_bindings[1].shader_stage = SHADER_STAGE::VERTEX;
			descriptor_bindings[1].type = DESCRIPTOR_TYPE::READONLY_BUFFER;

			descriptor_bindings[2].binding = GLOBAL_BINDLESS_TEXTURES_BINDING;
			descriptor_bindings[2].count = MAX_TEXTURES;
			descriptor_bindings[2].shader_stage = SHADER_STAGE::FRAGMENT_PIXEL;
			descriptor_bindings[2].type = DESCRIPTOR_TYPE::IMAGE;
			s_render_inst->global_descriptor_set = Vulkan::CreateDescriptorLayout(a_arena, Slice(descriptor_bindings, _countof(descriptor_bindings)));
			s_render_inst->global_descriptor_allocation = Vulkan::AllocateDescriptor(s_render_inst->global_descriptor_set);
		}
	}

	{
		GPUBufferCreateInfo vertex_buffer;
		vertex_buffer.name = "global vertex buffer";
		vertex_buffer.size = mbSize * 64;
		vertex_buffer.type = BUFFER_TYPE::STORAGE; //using byteaddressbuffer to get the vertices
		vertex_buffer.host_writable = false;

		s_render_inst->vertex_buffer.buffer = Vulkan::CreateBuffer(vertex_buffer);
		s_render_inst->vertex_buffer.size = static_cast<uint32_t>(vertex_buffer.size);
		s_render_inst->vertex_buffer.used = 0;

		vertex_buffer.host_writable = true;
		s_render_inst->cpu_vertex_buffer.buffer = Vulkan::CreateBuffer(vertex_buffer);
		s_render_inst->cpu_vertex_buffer.size = static_cast<uint32_t>(vertex_buffer.size);
		s_render_inst->cpu_vertex_buffer.used = 0;
		s_render_inst->cpu_vertex_buffer.start_mapped = Vulkan::MapBufferMemory(s_render_inst->cpu_vertex_buffer.buffer);

		GPUBufferView view;
		view.buffer = s_render_inst->vertex_buffer.buffer;
		view.offset = 0;
		view.size = s_render_inst->vertex_buffer.size;

		GPUBufferView cpu_view;
		cpu_view.buffer = s_render_inst->cpu_vertex_buffer.buffer;
		cpu_view.offset = 0;
		cpu_view.size = s_render_inst->cpu_vertex_buffer.size;

		WriteDescriptorData vertex_descriptor_write[2]{};
		vertex_descriptor_write[0].binding = GLOBAL_VERTEX_BUFFER_BINDING;
		vertex_descriptor_write[0].descriptor_index = 0;
		vertex_descriptor_write[0].type = DESCRIPTOR_TYPE::READONLY_BUFFER;
		vertex_descriptor_write[0].buffer_view = view;

		vertex_descriptor_write[1].binding = GLOBAL_CPU_VERTEX_BUFFER_BINDING;
		vertex_descriptor_write[1].descriptor_index = 0;
		vertex_descriptor_write[1].type = DESCRIPTOR_TYPE::READONLY_BUFFER;
		vertex_descriptor_write[1].buffer_view = cpu_view;

		WriteDescriptorInfos vertex_descriptor_info;
		vertex_descriptor_info.allocation = s_render_inst->global_descriptor_allocation;
		vertex_descriptor_info.descriptor_layout = s_render_inst->global_descriptor_set;
		vertex_descriptor_info.data = Slice(vertex_descriptor_write, _countof(vertex_descriptor_write));

		Vulkan::WriteDescriptors(vertex_descriptor_info);
	}
	{
		GPUBufferCreateInfo index_buffer;
		index_buffer.name = "global index buffer";
		index_buffer.size = mbSize * 64;
		index_buffer.type = BUFFER_TYPE::INDEX;
		index_buffer.host_writable = false;

		s_render_inst->index_buffer.buffer = Vulkan::CreateBuffer(index_buffer);
		s_render_inst->index_buffer.size = static_cast<uint32_t>(index_buffer.size);
		s_render_inst->index_buffer.used = 0;

		index_buffer.host_writable = true;
		s_render_inst->cpu_index_buffer.buffer = Vulkan::CreateBuffer(index_buffer);
		s_render_inst->cpu_index_buffer.size = static_cast<uint32_t>(index_buffer.size);
		s_render_inst->cpu_index_buffer.used = 0;
		s_render_inst->cpu_index_buffer.start_mapped = Vulkan::MapBufferMemory(s_render_inst->cpu_index_buffer.buffer);
	}

	//do some basic CPU-GPU upload that we need.
	CommandPool& start_up_pool = s_render_inst->graphics_queue.GetCommandPool("startup pool");
	const RCommandList list = start_up_pool.StartCommandList();

	const uint64_t asset_fence_value = GetNextAssetTransferFenceValueAndIncrement();

	{	//initialize texture system
		s_render_inst->texture_manager.Init(a_arena, list, asset_fence_value);

		//some basic colors
		const uint32_t white = UINT32_MAX;
		const uint32_t black = 0x000000FF;
		
		UploadTextureInfo image_info;
		image_info.name = "white";
		image_info.width = 1;
		image_info.height = 1;
		image_info.format = IMAGE_FORMAT::RGBA8_SRGB;
		image_info.pixels = &white;

		s_render_inst->white = UploadTexture(list, image_info, asset_fence_value);

		image_info.name = "black";
		image_info.pixels = &black;
		s_render_inst->black = UploadTexture(list, image_info, asset_fence_value);
	}

	{
		s_render_inst->frames = ArenaAllocArr(a_arena, RenderInterface_inst::Frame, s_render_inst->render_io.frame_count);
		for (size_t i = 0; i < s_render_inst->render_io.frame_count; i++)
		{
			const uint2 render_target_extent = { s_render_inst->render_io.screen_width, s_render_inst->render_io.screen_height };
			s_render_inst->frames[i].render_target = s_render_inst->texture_manager.CreateUploadTarget(render_target_extent, "image before transfer to swapchain");
			s_render_inst->frames[i].graphics_queue_fence_value = 0;
		}
	}

	{
		//per-frame descriptor set 1 for renderpass
		DescriptorBindingInfo descriptor_bindings[3];
		descriptor_bindings[0].binding = PER_SCENE_SCENE_DATA_BINDING;
		descriptor_bindings[0].count = 1;
		descriptor_bindings[0].shader_stage = SHADER_STAGE::ALL;
		descriptor_bindings[0].type = DESCRIPTOR_TYPE::READONLY_BUFFER;

		descriptor_bindings[1].binding = PER_SCENE_TRANSFORM_DATA_BINDING;
		descriptor_bindings[1].count = 1;
		descriptor_bindings[1].shader_stage = SHADER_STAGE::VERTEX;
		descriptor_bindings[1].type = DESCRIPTOR_TYPE::READONLY_BUFFER;

		descriptor_bindings[2].binding = PER_SCENE_LIGHT_DATA_BINDING;
		descriptor_bindings[2].count = 1;
		descriptor_bindings[2].shader_stage = SHADER_STAGE::FRAGMENT_PIXEL;
		descriptor_bindings[2].type = DESCRIPTOR_TYPE::READONLY_BUFFER;
		s_render_inst->scene3d_descriptor_layout = Vulkan::CreateDescriptorLayout(a_arena, Slice(descriptor_bindings, _countof(descriptor_bindings)));
	}
	
	IMGUI_IMPL::ImInit(a_arena, list, s_render_inst->asset_upload_next_fence_value);

	s_render_inst->gpu_device = Vulkan::GetGPUDeviceInfo(a_arena);

	start_up_pool.EndCommandList(list);
	//special upload here, due to uploading as well
	s_render_inst->graphics_queue.ReturnPool(start_up_pool);
	s_render_inst->graphics_queue.ExecuteCommands(&list, 1, &s_render_inst->asset_upload_fence, &asset_fence_value, 1, nullptr, nullptr, 0);
	return true;
}

void BB::RequestResize()
{
	s_render_inst->render_io.resizing_request = true;
}

void BB::GPUWaitIdle()
{
	s_render_inst->transfer_queue.WaitIdle();
	s_render_inst->graphics_queue.WaitIdle();
	// TODO, compute queue wait idle
	// s_render_inst->compute_queue.WaitIdle();
}

static void ResizeRendererSwapchain(const uint32_t a_width, const uint32_t a_height)
{
	s_render_inst->graphics_queue.WaitIdle();
	Vulkan::RecreateSwapchain(a_width, a_height);
	s_render_inst->render_io.screen_width = a_width;
	s_render_inst->render_io.screen_height = a_height;

	for (size_t i = 0; i < s_render_inst->render_io.frame_count; i++)
	{
		FreeTexture(s_render_inst->frames[i].render_target);

		const uint2 render_target_extent = { s_render_inst->render_io.screen_width, s_render_inst->render_io.screen_height };
		s_render_inst->frames[i].render_target = s_render_inst->texture_manager.CreateUploadTarget(render_target_extent, "image before transfer to swapchain");
		s_render_inst->frames[i].graphics_queue_fence_value = 0;
	}

	// also reset the render debug state
	s_render_inst->render_io.frame_started = false;
	s_render_inst->render_io.frame_ended = false;
	s_render_inst->render_io.resizing_request = false;
}

void BB::StartFrame(const RCommandList a_list)
{
	// check if we need to resize
	if (s_render_inst->render_io.resizing_request)
	{
		int x, y;
		GetWindowSize(s_render_inst->render_io.window_handle, x, y);
		ResizeRendererSwapchain(static_cast<uint32_t>(x), static_cast<uint32_t>(y));
	}

	BB_ASSERT(s_render_inst->render_io.frame_started == false, "did not call EndFrame before a new StartFrame");
	s_render_inst->render_io.frame_started = true;

	const uint32_t frame_index = s_render_inst->render_io.frame_index;
	const RenderInterface_inst::Frame& cur_frame = s_render_inst->frames[frame_index];

	s_render_inst->graphics_queue.WaitFenceValue(cur_frame.graphics_queue_fence_value);

	const GPUTextureManager::TextureSlot render_target = s_render_inst->texture_manager.GetTextureSlot(cur_frame.render_target);
	{
		PipelineBarrierImageInfo image_transitions[1]{};
		image_transitions[0].src_mask = BARRIER_ACCESS_MASK::NONE;
		image_transitions[0].dst_mask = BARRIER_ACCESS_MASK::COLOR_ATTACHMENT_WRITE;
		image_transitions[0].image = render_target.texture_info.image;
		image_transitions[0].old_layout = IMAGE_LAYOUT::UNDEFINED;
		image_transitions[0].new_layout = IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL;
		image_transitions[0].layer_count = 1;
		image_transitions[0].level_count = 1;
		image_transitions[0].base_array_layer = 0;
		image_transitions[0].base_mip_level = 0;
		image_transitions[0].src_stage = BARRIER_PIPELINE_STAGE::TOP_OF_PIPELINE;
		image_transitions[0].dst_stage = BARRIER_PIPELINE_STAGE::COLOR_ATTACH_OUTPUT;

		PipelineBarrierInfo pipeline_info{};
		pipeline_info.image_info_count = _countof(image_transitions);
		pipeline_info.image_infos = image_transitions;
		Vulkan::PipelineBarriers(a_list, pipeline_info);
	}

	IMGUI_IMPL::ImNewFrame();
	ImGui::NewFrame();

	ImguiDisplayRenderer();
}

void BB::EndFrame(const RCommandList a_list, bool a_skip)
{
	BB_ASSERT(s_render_inst->render_io.frame_started == true, "did not call StartFrame before a EndFrame");

	if (a_skip || s_render_inst->render_io.resizing_request)
	{
		ImGui::EndFrame();
		return;
	}
	const uint32_t frame_index = s_render_inst->render_io.frame_index;
	const RenderInterface_inst::Frame& cur_frame = s_render_inst->frames[frame_index];
	const GPUTextureManager::TextureSlot render_target = s_render_inst->texture_manager.GetTextureSlot(cur_frame.render_target);

	ImGui::Render();
	IMGUI_IMPL::ImRenderFrame(a_list, render_target.texture_info.view, true);
	ImGui::EndFrame();

	{
		PipelineBarrierImageInfo image_transitions[1]{};
		image_transitions[0].src_mask = BARRIER_ACCESS_MASK::COLOR_ATTACHMENT_WRITE;
		image_transitions[0].dst_mask = BARRIER_ACCESS_MASK::TRANSFER_READ;
		image_transitions[0].image = render_target.texture_info.image;
		image_transitions[0].old_layout = IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL;
		image_transitions[0].new_layout = IMAGE_LAYOUT::TRANSFER_SRC;
		image_transitions[0].layer_count = 1;
		image_transitions[0].level_count = 1;
		image_transitions[0].base_array_layer = 0;
		image_transitions[0].base_mip_level = 0;
		image_transitions[0].src_stage = BARRIER_PIPELINE_STAGE::COLOR_ATTACH_OUTPUT;
		image_transitions[0].dst_stage = BARRIER_PIPELINE_STAGE::TRANSFER;

		PipelineBarrierInfo pipeline_info{};
		pipeline_info.image_info_count = _countof(image_transitions);
		pipeline_info.image_infos = image_transitions;
		Vulkan::PipelineBarriers(a_list, pipeline_info);
	}

	s_render_inst->texture_manager.TransitionGraphicsQueueTexture(a_list);

	const int2 swapchain_size =
	{
		{
		static_cast<int>(s_render_inst->render_io.screen_width),
		static_cast<int>(s_render_inst->render_io.screen_height)
		}
	};
	const PRESENT_IMAGE_RESULT result = Vulkan::UploadImageToSwapchain(a_list, render_target.texture_info.image, swapchain_size, swapchain_size, s_render_inst->render_io.frame_index);
	if (result == PRESENT_IMAGE_RESULT::SWAPCHAIN_OUT_OF_DATE)
		s_render_inst->render_io.resizing_request = true;
	s_render_inst->render_io.frame_ended = true;
}

struct RenderTargetStruct
{
	RTexture render_targets[3]; // same amount as back buffer amount
	uint2 extent;
	const char* name;

	RTexture GetTargetTexture() const { return render_targets[s_render_inst->render_io.frame_index]; }
};

RenderTarget BB::CreateRenderTarget(MemoryArena& a_arena, const uint2 a_render_target_extent, const char* a_name)
{
	RenderTargetStruct* viewport = ArenaAllocType(a_arena, RenderTargetStruct);
	for (uint32_t i = 0; i < s_render_inst->render_io.frame_count; i++)
	{
		viewport->render_targets[i] = s_render_inst->texture_manager.CreateRenderTarget(a_render_target_extent, a_name);
	}
	viewport->extent = a_render_target_extent;
	viewport->name = a_name;
	return RenderTarget(reinterpret_cast<uintptr_t>(viewport));
}

void BB::ResizeRenderTarget(const RenderTarget render_target, const uint2 a_render_target_extent)
{
	RenderTargetStruct* viewport = reinterpret_cast<RenderTargetStruct*>(render_target.handle);
	
	// we can't garuntee that the viewport is not being worked on or used in copy commands, so wait all commands.
	s_render_inst->graphics_queue.WaitIdle();

	for (uint32_t i = 0; i < s_render_inst->render_io.frame_count; i++)
	{
		FreeTexture(viewport->render_targets[i]);
		viewport->render_targets[i] = s_render_inst->texture_manager.CreateRenderTarget(a_render_target_extent, viewport->name);
	}
	viewport->extent = a_render_target_extent;
}

void BB::StartRenderTarget(const RCommandList a_list, const RenderTarget a_render_target)
{
	const RTexture render_target = reinterpret_cast<RenderTargetStruct*>(a_render_target.handle)->GetTargetTexture();

	GPUTextureManager::TextureSlot& slot = s_render_inst->texture_manager.GetTextureSlot(render_target);
	{
		PipelineBarrierImageInfo render_target_transition;
		render_target_transition.src_mask = BARRIER_ACCESS_MASK::NONE;
		render_target_transition.dst_mask = BARRIER_ACCESS_MASK::COLOR_ATTACHMENT_WRITE;
		render_target_transition.image = slot.texture_info.image;
		render_target_transition.old_layout = IMAGE_LAYOUT::UNDEFINED;
		render_target_transition.new_layout = IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL;
		render_target_transition.layer_count = 1;
		render_target_transition.level_count = 1;
		render_target_transition.base_array_layer = 0;
		render_target_transition.base_mip_level = 0;
		render_target_transition.src_stage = BARRIER_PIPELINE_STAGE::TOP_OF_PIPELINE;
		render_target_transition.dst_stage = BARRIER_PIPELINE_STAGE::COLOR_ATTACH_OUTPUT;

		PipelineBarrierInfo pipeline_info{};
		pipeline_info.image_info_count = 1;
		pipeline_info.image_infos = &render_target_transition;
		Vulkan::PipelineBarriers(a_list, pipeline_info);

		slot.texture_info.current_layout = render_target_transition.new_layout;
	}
}

void BB::EndRenderTarget(const RCommandList a_list, const RenderTarget a_render_target)
{
	const RTexture render_target = reinterpret_cast<RenderTargetStruct*>(a_render_target.handle)->GetTargetTexture();

	GPUTextureManager::TextureSlot& slot = s_render_inst->texture_manager.GetTextureSlot(render_target);
	{
		PipelineBarrierImageInfo render_target_transition;
		render_target_transition.src_mask = BARRIER_ACCESS_MASK::COLOR_ATTACHMENT_WRITE;
		render_target_transition.dst_mask = BARRIER_ACCESS_MASK::SHADER_READ;
		render_target_transition.image = slot.texture_info.image;
		render_target_transition.old_layout = slot.texture_info.current_layout;
		render_target_transition.new_layout = IMAGE_LAYOUT::SHADER_READ_ONLY;
		render_target_transition.layer_count = 1;
		render_target_transition.level_count = 1;
		render_target_transition.base_array_layer = 0;
		render_target_transition.base_mip_level = 0;
		render_target_transition.src_stage = BARRIER_PIPELINE_STAGE::COLOR_ATTACH_OUTPUT;
		render_target_transition.dst_stage = BARRIER_PIPELINE_STAGE::FRAGMENT_SHADER;

		PipelineBarrierInfo pipeline_info{};
		pipeline_info.image_info_count = 1;
		pipeline_info.image_infos = &render_target_transition;
		Vulkan::PipelineBarriers(a_list, pipeline_info);

		slot.texture_info.current_layout = render_target_transition.new_layout;
	}
}

RTexture BB::GetCurrentRenderTargetTexture(const RenderTarget a_render_target)
{
	return reinterpret_cast<RenderTargetStruct*>(a_render_target.handle)->GetTargetTexture();
}

RenderScene3DHandle BB::Create3DRenderScene(MemoryArena& a_arena, const SceneCreateInfo& a_info, const char* a_name)
{
	Scene3D* scene_3d = ArenaAllocType(a_arena, Scene3D);

	scene_3d->scene_name = a_name;
	scene_3d->scene_info.ambient_light = a_info.ambient_light_color;
	scene_3d->scene_info.ambient_strength = a_info.ambient_light_strength;

	scene_3d->frames = ArenaAllocArr(a_arena, Scene3D::Frame, s_render_inst->render_io.frame_count);
	scene_3d->draw_list_max = a_info.draw_entry_max;
	scene_3d->draw_list_count = 0;
	scene_3d->draw_list_data.mesh_draw_call = ArenaAllocArr(a_arena, MeshDrawCall, scene_3d->draw_list_max);
	scene_3d->draw_list_data.transform = ArenaAllocArr(a_arena, ShaderTransform, scene_3d->draw_list_max);

	scene_3d->previous_draw_area = uint2(0, 0);

	scene_3d->light_container.Init(a_arena, a_info.light_max);

	constexpr uint32_t scene_size = sizeof(Scene3DInfo);
	const uint32_t shader_transform_size = scene_3d->draw_list_max * sizeof(ShaderTransform);
	const uint32_t light_buffer_size = scene_3d->light_container.capacity() * sizeof(PointLight);

	const uint32_t per_frame_buffer_size = scene_size + shader_transform_size + light_buffer_size;

	//per frame stuff
	GPUBufferCreateInfo per_frame_buffer_info;
	per_frame_buffer_info.name = "per_frame_buffer";
	per_frame_buffer_info.size = per_frame_buffer_size;
	per_frame_buffer_info.type = BUFFER_TYPE::STORAGE;

	for (uint32_t i = 0; i < s_render_inst->render_io.frame_count; i++)
	{
		Scene3D::Frame& pf = scene_3d->frames[i];
		{
			pf.per_frame_buffer = Vulkan::CreateBuffer(per_frame_buffer_info);
			pf.per_frame_buffer_size = static_cast<uint32_t>(per_frame_buffer_info.size);
		}

		uint32_t buffer_used = 0;
		{
			pf.scene_buffer.size = scene_size;
			pf.scene_buffer.offset = buffer_used;

			buffer_used += static_cast<uint32_t>(pf.scene_buffer.size);
		}
		{
			pf.transform_buffer.size = shader_transform_size;
			pf.transform_buffer.offset = buffer_used;

			buffer_used += static_cast<uint32_t>(pf.transform_buffer.size);
		}
		{
			pf.light_buffer.size = light_buffer_size;
			pf.light_buffer.offset = buffer_used;

			buffer_used += static_cast<uint32_t>(pf.light_buffer.size);
		}

		//descriptors
		pf.desc_alloc = Vulkan::AllocateDescriptor(s_render_inst->scene3d_descriptor_layout);

		WriteDescriptorData per_scene_buffer_desc[3]{};
		per_scene_buffer_desc[0].binding = PER_SCENE_SCENE_DATA_BINDING;
		per_scene_buffer_desc[0].descriptor_index = 0;
		per_scene_buffer_desc[0].type = DESCRIPTOR_TYPE::READONLY_BUFFER;
		per_scene_buffer_desc[0].buffer_view.buffer = pf.per_frame_buffer;
		per_scene_buffer_desc[0].buffer_view.size = pf.scene_buffer.size;
		per_scene_buffer_desc[0].buffer_view.offset = pf.scene_buffer.offset;

		per_scene_buffer_desc[1].binding = PER_SCENE_TRANSFORM_DATA_BINDING;
		per_scene_buffer_desc[1].descriptor_index = 0;
		per_scene_buffer_desc[1].type = DESCRIPTOR_TYPE::READONLY_BUFFER;
		per_scene_buffer_desc[1].buffer_view.buffer = pf.per_frame_buffer;
		per_scene_buffer_desc[1].buffer_view.size = pf.transform_buffer.size;
		per_scene_buffer_desc[1].buffer_view.offset = pf.transform_buffer.offset;

		per_scene_buffer_desc[2].binding = PER_SCENE_LIGHT_DATA_BINDING;
		per_scene_buffer_desc[2].descriptor_index = 0;
		per_scene_buffer_desc[2].type = DESCRIPTOR_TYPE::READONLY_BUFFER;
		per_scene_buffer_desc[2].buffer_view.buffer = pf.per_frame_buffer;
		per_scene_buffer_desc[2].buffer_view.size = pf.light_buffer.size;
		per_scene_buffer_desc[2].buffer_view.offset = pf.light_buffer.offset;

		WriteDescriptorInfos frame_desc_write;
		frame_desc_write.allocation = pf.desc_alloc;
		frame_desc_write.descriptor_layout = s_render_inst->scene3d_descriptor_layout;
		frame_desc_write.data = Slice(per_scene_buffer_desc, _countof(per_scene_buffer_desc));

		Vulkan::WriteDescriptors(frame_desc_write);
	}

	return RenderScene3DHandle(reinterpret_cast<uintptr_t>(scene_3d));
}

void BB::StartRenderScene(const RenderScene3DHandle a_scene)
{
	Scene3D& render_scene3d = *reinterpret_cast<Scene3D*>(a_scene.handle);
	s_render_inst->graphics_queue.WaitFenceValue(render_scene3d.frames[s_render_inst->render_io.frame_index].fence_value);

	//clear the drawlist, maybe do this on a per-frame basis of we do GPU upload?
	render_scene3d.draw_list_count = 0;
}

void BB::EndRenderScene(const RCommandList a_cmd_list, const RenderScene3DHandle a_scene, const RenderTarget a_render_target, const uint2 a_draw_area_size, const int2 a_draw_area_offset, const float3 a_clear_color, bool a_skip)
{
	Scene3D& render_scene3d = *reinterpret_cast<Scene3D*>(a_scene.handle);
	const auto& scene_frame = render_scene3d.frames[s_render_inst->render_io.frame_index];

	if (render_scene3d.draw_list_count == 0 || a_skip)
	{
		return;
	}

	render_scene3d.scene_info.light_count = render_scene3d.light_container.size();

	const size_t scene_upload_size = sizeof(Scene3DInfo);
	const size_t matrices_upload_size = sizeof(ShaderTransform) * render_scene3d.draw_list_count;
	const size_t light_upload_size = sizeof(Light) * render_scene3d.light_container.capacity();
	// optimize this
	const size_t total_size = scene_upload_size + matrices_upload_size + light_upload_size;

	const UploadBuffer upload_buffer = s_render_inst->frame_upload_allocator.AllocateUploadMemory(total_size, scene_frame.fence_value + 1);

	size_t bytes_uploaded = 0;
	upload_buffer.SafeMemcpy(bytes_uploaded, &render_scene3d.scene_info, scene_upload_size);
	const size_t scene_offset = bytes_uploaded + upload_buffer.base_offset;
	bytes_uploaded += scene_upload_size;

	upload_buffer.SafeMemcpy(bytes_uploaded, render_scene3d.draw_list_data.transform, matrices_upload_size);
	const size_t matrix_offset = bytes_uploaded + upload_buffer.base_offset;
	bytes_uploaded += matrices_upload_size;

	upload_buffer.SafeMemcpy(bytes_uploaded, render_scene3d.light_container.data(), light_upload_size);
	const size_t light_offset = bytes_uploaded + upload_buffer.base_offset;
	bytes_uploaded += light_upload_size;

	//upload to some GPU buffer here.
	RenderCopyBuffer matrix_buffer_copy;
	matrix_buffer_copy.src = upload_buffer.buffer;
	matrix_buffer_copy.dst = scene_frame.per_frame_buffer;
	RenderCopyBufferRegion buffer_regions[3]; // 0 = scene, 1 = matrix
	buffer_regions[0].src_offset = scene_offset;
	buffer_regions[0].dst_offset = scene_frame.scene_buffer.offset;
	buffer_regions[0].size = scene_frame.scene_buffer.size;

	buffer_regions[1].src_offset = matrix_offset;
	buffer_regions[1].dst_offset = scene_frame.transform_buffer.offset;
	buffer_regions[1].size = matrices_upload_size;

	buffer_regions[2].src_offset = light_offset;
	buffer_regions[2].dst_offset = scene_frame.light_buffer.offset;
	buffer_regions[2].size = light_upload_size;
	matrix_buffer_copy.regions = Slice(buffer_regions, _countof(buffer_regions));
	Vulkan::CopyBuffer(a_cmd_list, matrix_buffer_copy);

	if (render_scene3d.previous_draw_area != a_draw_area_size)
	{
		if (render_scene3d.depth_image.IsValid())
		{
			Vulkan::FreeViewImage(render_scene3d.depth_image_view);
			Vulkan::FreeImage(render_scene3d.depth_image);
		}

		RenderDepthCreateInfo depth_create_info;
		depth_create_info.name = "standard depth buffer";
		depth_create_info.width = a_draw_area_size.x;
		depth_create_info.height = a_draw_area_size.y;
		depth_create_info.depth = 1;
		depth_create_info.depth_format = DEPTH_FORMAT::D24_UNORM_S8_UINT;
		Vulkan::CreateDepthBuffer(depth_create_info, render_scene3d.depth_image, render_scene3d.depth_image_view);
		render_scene3d.previous_draw_area = a_draw_area_size;
	}

	//transition depth buffer
	{
		//pipeline barrier
		//0 = color image, 1 = depth image
		PipelineBarrierImageInfo image_transitions[1]{};

		image_transitions[0].src_mask = BARRIER_ACCESS_MASK::NONE;
		image_transitions[0].dst_mask = BARRIER_ACCESS_MASK::DEPTH_STENCIL_READ_WRITE;
		image_transitions[0].image = render_scene3d.depth_image;
		image_transitions[0].old_layout = IMAGE_LAYOUT::UNDEFINED;
		image_transitions[0].new_layout = IMAGE_LAYOUT::DEPTH_STENCIL_ATTACHMENT;
		image_transitions[0].layer_count = 1;
		image_transitions[0].level_count = 1;
		image_transitions[0].base_array_layer = 0;
		image_transitions[0].base_mip_level = 0;
		image_transitions[0].src_stage = BARRIER_PIPELINE_STAGE::FRAGMENT_TEST;
		image_transitions[0].dst_stage = BARRIER_PIPELINE_STAGE::FRAGMENT_TEST;

		PipelineBarrierInfo pipeline_info{};
		pipeline_info.image_info_count = _countof(image_transitions);
		pipeline_info.image_infos = image_transitions;
		Vulkan::PipelineBarriers(a_cmd_list, pipeline_info);
	}
	const RTexture render_target_texture = reinterpret_cast<RenderTargetStruct*>(a_render_target.handle)->GetTargetTexture();
	const GPUTextureManager::TextureSlot render_target = s_render_inst->texture_manager.GetTextureSlot(render_target_texture);
	// render
	StartRenderingInfo start_rendering_info;
	start_rendering_info.viewport_size = a_draw_area_size;
	start_rendering_info.scissor_extent = a_draw_area_size;
	start_rendering_info.scissor_offset = a_draw_area_offset;
	start_rendering_info.depth_view = render_scene3d.depth_image_view;
	start_rendering_info.layout = IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL;
	start_rendering_info.load_color = false;
	start_rendering_info.store_color = true;
	start_rendering_info.clear_color_rgba = float4{ a_clear_color.x, a_clear_color.y, a_clear_color.z, 1.f };
	Vulkan::StartRenderPass(a_cmd_list, start_rendering_info, render_target.texture_info.view);

	{
		//set the first data to get the first 3 descriptor sets.
		const MeshDrawCall& mesh_draw_call = render_scene3d.draw_list_data.mesh_draw_call[0];
		const Material& material = s_render_inst->material_map.find(mesh_draw_call.material);

		//set 0
		Vulkan::SetDescriptorImmutableSamplers(a_cmd_list, material.shader.pipeline_layout);
		const uint32_t buffer_indices[] = { 0, 0 };
		const size_t buffer_offsets[]{ s_render_inst->global_descriptor_allocation.offset, scene_frame.desc_alloc.offset };
		//set 1-2
		Vulkan::SetDescriptorBufferOffset(a_cmd_list,
			material.shader.pipeline_layout,
			SPACE_GLOBAL, 
			_countof(buffer_offsets),
			buffer_indices,
			buffer_offsets);
	}

	Vulkan::BindIndexBuffer(a_cmd_list, s_render_inst->index_buffer.buffer, 0);

	for (uint32_t i = 0; i < render_scene3d.draw_list_count; i++)
	{
		const MeshDrawCall& mesh_draw_call = render_scene3d.draw_list_data.mesh_draw_call[i];
		const Material& material = s_render_inst->material_map.find(mesh_draw_call.material);
		const Mesh& mesh = s_render_inst->mesh_map.find(mesh_draw_call.mesh);

		Vulkan::BindShaders(a_cmd_list,
			material.shader.shader_effect_count,
			material.shader.shader_stages,
			material.shader.shader_objects);

		ShaderIndices shader_indices;
		shader_indices.transform_index = i;
		shader_indices.vertex_buffer_offset = static_cast<uint32_t>(mesh.vertex_buffer.offset);
		shader_indices.albedo_texture = material.base_color.handle;

		Vulkan::SetPushConstants(a_cmd_list, material.shader.pipeline_layout, 0, sizeof(ShaderIndices), &shader_indices);

		Vulkan::DrawIndexed(a_cmd_list,
			static_cast<uint32_t>(mesh.index_buffer.size / sizeof(uint32_t)) + mesh_draw_call.index_count,
			1,
			static_cast<uint32_t>(mesh.index_buffer.offset / sizeof(uint32_t)) + mesh_draw_call.index_start,
			0,
			0);
	}

	Vulkan::EndRenderPass(a_cmd_list);
}

void BB::SetView(const RenderScene3DHandle a_scene, const float4x4& a_view)
{
	Scene3D& render_scene3d = *reinterpret_cast<Scene3D*>(a_scene.handle);
	render_scene3d.scene_info.view = a_view;
}

void BB::SetProjection(const RenderScene3DHandle a_scene, const float4x4& a_proj)
{
	Scene3D& render_scene3d = *reinterpret_cast<Scene3D*>(a_scene.handle);
	render_scene3d.scene_info.proj = a_proj;
}

CommandPool& BB::GetGraphicsCommandPool()
{
	return s_render_inst->graphics_queue.GetCommandPool();
}

CommandPool& BB::GetTransferCommandPool()
{
	return s_render_inst->transfer_queue.GetCommandPool();
}

bool BB::PresentFrame(const BB::Slice<CommandPool> a_cmd_pools)
{
	if (s_render_inst->render_io.resizing_request)
	{
		s_render_inst->graphics_queue.ReturnPools(a_cmd_pools);
		return false;
	}

	BB_ASSERT(s_render_inst->render_io.frame_started == true, "did not call StartFrame before a presenting");
	BB_ASSERT(s_render_inst->render_io.frame_ended == true, "did not call EndFrame before a presenting");

	s_render_inst->render_io.frame_started = true;

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
	s_render_inst->frames[s_render_inst->render_io.frame_index].graphics_queue_fence_value = s_render_inst->graphics_queue.GetNextFenceValue();

	s_render_inst->graphics_queue.ReturnPools(a_cmd_pools);
	const PRESENT_IMAGE_RESULT result = s_render_inst->graphics_queue.ExecutePresentCommands(lists, list_count, nullptr, nullptr, 0, nullptr, nullptr, 0, s_render_inst->render_io.frame_index);
	s_render_inst->render_io.frame_index = (s_render_inst->render_io.frame_index + 1) % s_render_inst->render_io.frame_count;

	s_render_inst->render_io.frame_ended = false;
	s_render_inst->render_io.frame_started = false;

	if (result == PRESENT_IMAGE_RESULT::SWAPCHAIN_OUT_OF_DATE)
		s_render_inst->render_io.resizing_request = true;

	return true;
}

bool BB::ExecuteGraphicCommands(const BB::Slice<CommandPool> a_cmd_pools)
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

	s_render_inst->graphics_queue.ReturnPools(a_cmd_pools);
	s_render_inst->graphics_queue.ExecuteCommands(lists, list_count, nullptr, nullptr, 0, nullptr, nullptr, 0);
	return true;
}

uint64_t BB::GetNextAssetTransferFenceValueAndIncrement()
{
	OSAcquireSRWLockWrite(&s_render_inst->asset_upload_value_lock);
	const uint64_t fence_value = s_render_inst->asset_upload_next_fence_value++;
	OSReleaseSRWLockWrite(&s_render_inst->asset_upload_value_lock);
	return fence_value;
}

bool BB::ExecuteAssetTransfer(const BB::Slice<CommandPool> a_cmd_pools, const uint64_t a_asset_transfer_fence_value)
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

	s_render_inst->transfer_queue.ReturnPools(a_cmd_pools);
	s_render_inst->transfer_queue.ExecuteCommands(lists, list_count, &s_render_inst->asset_upload_fence, &a_asset_transfer_fence_value, 1, nullptr, nullptr, 0);
	return true;
}

const MeshHandle BB::CreateMesh(const RCommandList a_list, const CreateMeshInfo& a_create_info, const uint64_t a_transfer_fence_value)
{
	UploadBuffer upload_buffer = s_render_inst->asset_upload_allocator.AllocateUploadMemory(a_create_info.vertices.sizeInBytes() + a_create_info.indices.sizeInBytes(), a_transfer_fence_value);

	Mesh mesh;
	mesh.vertex_buffer = AllocateFromVertexBuffer(a_create_info.vertices.sizeInBytes());
	mesh.index_buffer = AllocateFromIndexBuffer(a_create_info.indices.sizeInBytes());


	const size_t vertex_offset = 0;
	const size_t index_offset = a_create_info.vertices.sizeInBytes();
	upload_buffer.SafeMemcpy(vertex_offset, a_create_info.vertices.data(), a_create_info.vertices.sizeInBytes());
	upload_buffer.SafeMemcpy(index_offset, a_create_info.indices.data(), a_create_info.indices.sizeInBytes());

	RenderCopyBufferRegion copy_regions[2];
	RenderCopyBuffer copy_buffer_infos[2];

	copy_buffer_infos[0].dst = mesh.vertex_buffer.buffer;
	copy_buffer_infos[0].src = upload_buffer.buffer;
	copy_buffer_infos[0].regions = Slice(&copy_regions[0], 1);
	copy_regions[0].size = mesh.vertex_buffer.size;
	copy_regions[0].dst_offset = mesh.vertex_buffer.offset;
	copy_regions[0].src_offset = vertex_offset + upload_buffer.base_offset;

	copy_buffer_infos[1].dst = mesh.index_buffer.buffer;
	copy_buffer_infos[1].src = upload_buffer.buffer;
	copy_buffer_infos[1].regions = Slice(&copy_regions[1], 1);
	copy_regions[1].size = mesh.index_buffer.size;
	copy_regions[1].dst_offset = mesh.index_buffer.offset;
	copy_regions[1].src_offset = index_offset + upload_buffer.base_offset;

	Vulkan::CopyBuffers(a_list, copy_buffer_infos, 2);

	return MeshHandle(s_render_inst->mesh_map.insert(mesh).handle);
}

void BB::FreeMesh(const MeshHandle a_mesh)
{
	s_render_inst->mesh_map.erase(a_mesh);
}

LightHandle BB::CreateLight(const RenderScene3DHandle a_scene, const CreateLightInfo& a_create_info)
{
	Scene3D& render_scene3d = *reinterpret_cast<Scene3D*>(a_scene.handle);

	PointLight light;
	light.pos = a_create_info.pos;
	light.color = a_create_info.color;
	light.radius_linear = a_create_info.linear_distance;
	light.radius_quadratic = a_create_info.quadratic_distance;
	return render_scene3d.light_container.insert(light);
}

void BB::CreateLights(const RenderScene3DHandle a_scene, const Slice<CreateLightInfo> a_create_infos, LightHandle* const a_light_handles)
{
	Scene3D& render_scene3d = *reinterpret_cast<Scene3D*>(a_scene.handle);

	for (size_t i = 0; i < a_create_infos.size(); i++)
	{
		PointLight light;
		light.pos = a_create_infos[i].pos;
		light.color = a_create_infos[i].color;
		light.radius_linear = a_create_infos[i].linear_distance;
		light.radius_quadratic = a_create_infos[i].quadratic_distance;
		a_light_handles[i] = render_scene3d.light_container.insert(light);
	}
}

void BB::FreeLight(const RenderScene3DHandle a_scene, const LightHandle a_light)
{
	Scene3D& render_scene3d = *reinterpret_cast<Scene3D*>(a_scene.handle);
	render_scene3d.light_container.find(a_light) = {0};
	render_scene3d.light_container.erase(a_light);
}

PointLight& BB::GetLight(const RenderScene3DHandle a_scene, const LightHandle a_light)
{
	const Scene3D& render_scene3d = *reinterpret_cast<const Scene3D*>(a_scene.handle);
	return render_scene3d.light_container.find(a_light);
}

bool BB::CreateShaderEffect(MemoryArena& a_temp_arena, const Slice<CreateShaderEffectInfo> a_create_infos, ShaderEffectHandle* const a_handles)
{
	// our default layouts
	// array size should be SPACE_AMOUNT
	FixedArray<RDescriptorLayout, SPACE_AMOUNT> desc_layouts = {
			s_render_inst->static_sampler_descriptor_set,
			s_render_inst->global_descriptor_set,
			RDescriptorLayout(BB_INVALID_HANDLE_64),	// SCENE SET
			RDescriptorLayout(BB_INVALID_HANDLE_64),	// MATERIAL SET, NOT USED
			RDescriptorLayout(BB_INVALID_HANDLE_64) };	// OBJECT SET, NOT USED

	// all of them use this push constant for the shader indices.
	PushConstantRange push_constant;
	push_constant.stages = SHADER_STAGE::ALL;
	push_constant.offset = 0;

	ShaderEffect* shader_effects = ArenaAllocArr(a_temp_arena, ShaderEffect, a_create_infos.size());
	ShaderCode* shader_codes = ArenaAllocArr(a_temp_arena, ShaderCode, a_create_infos.size());
	ShaderObjectCreateInfo* shader_object_infos = ArenaAllocArr(a_temp_arena, ShaderObjectCreateInfo, a_create_infos.size());

	for (size_t i = 0; i < a_create_infos.size(); i++)
	{
		switch (a_create_infos[i].pass_type)
		{
		case RENDER_PASS_TYPE::STANDARD_3D:
			desc_layouts[SPACE_PER_SCENE] = s_render_inst->scene3d_descriptor_layout;
			break;
		default:
			BB_ASSERT(false, "Unsupported/Unimplemented RENDER_PASS_TYPE");
			break;
		}

		push_constant.size = a_create_infos[i].push_constant_space;
		shader_effects[i].pipeline_layout = Vulkan::CreatePipelineLayout(desc_layouts.data(), 3, &push_constant, 1);

		shader_codes[i] = CompileShader(s_render_inst->shader_compiler,
			a_create_infos[i].shader_path,
			a_create_infos[i].shader_entry,
			a_create_infos[i].stage);
		Buffer shader_buffer = GetShaderCodeBuffer(shader_codes[i]);

		shader_object_infos[i].stage = a_create_infos[i].stage;
		shader_object_infos[i].next_stages = a_create_infos[i].next_stages;
		shader_object_infos[i].shader_code_size = shader_buffer.size;
		shader_object_infos[i].shader_code = shader_buffer.data;
		shader_object_infos[i].shader_entry = a_create_infos[i].shader_entry;

		shader_object_infos[i].descriptor_layout_count = 3;
		shader_object_infos[i].descriptor_layouts = desc_layouts;
		shader_object_infos[i].push_constant_range = push_constant;
	}

	ShaderObject* shader_objects = ArenaAllocArr(a_temp_arena, ShaderObject, a_create_infos.size());
	Vulkan::CreateShaderObjects(a_temp_arena, Slice(shader_object_infos, a_create_infos.size()), shader_objects);

	for (size_t i = 0; i < a_create_infos.size(); i++)
	{
		shader_effects[i].name = shader_effects[i].name;
		shader_effects[i].shader_object = shader_objects[i];
		shader_effects[i].shader_stage = a_create_infos[i].stage;
		shader_effects[i].shader_stages_next = a_create_infos[i].next_stages;
#ifdef _ENABLE_REBUILD_SHADERS
		shader_effects[i].create_info = shader_object_infos[i];
		shader_effects[i].shader_path = a_create_infos[i].shader_path;
		shader_effects[i].shader_entry = a_create_infos[i].shader_entry;
#endif // _ENABLE_REBUILD_SHADERS

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

bool BB::ReloadShaderEffect(const ShaderEffectHandle a_shader_effect)
{
#ifdef _ENABLE_REBUILD_SHADERS
	ShaderEffect& old_effect = s_render_inst->shader_effect_map.find(a_shader_effect);
	Vulkan::DestroyShaderObject(old_effect.shader_object);

	const ShaderCode shader_code = CompileShader(s_render_inst->shader_compiler,
		old_effect.shader_path,
		old_effect.shader_entry,
		old_effect.shader_stage);

	const Buffer shader_data = GetShaderCodeBuffer(shader_code);
	old_effect.create_info.shader_code = shader_data.data;
	old_effect.create_info.shader_code_size = shader_data.size;
	old_effect.create_info.shader_entry = old_effect.shader_entry;

	const ShaderObject new_object = Vulkan::CreateShaderObject(old_effect.create_info);
	old_effect.shader_object = new_object;

	ReleaseShaderCode(shader_code);

	return new_object.IsValid();
#endif // _ENABLE_REBUILD_SHADERS
	BB_WARNING(false, "trying to reload a shader but _ENABLE_REBUILD_SHADERS is not defined", WarningType::MEDIUM);
	return true;
}

const MaterialHandle BB::CreateMaterial(const CreateMaterialInfo& a_create_info)
{
	BB_ASSERT(UNIQUE_SHADER_STAGE_COUNT >= a_create_info.shader_effects.size(), "too many shader stages!");
	BB_ASSERT(UNIQUE_SHADER_STAGE_COUNT != 0, "no shader effects in material!");

	Material mat;
	// get the first pipeline layout, compare it with all of the ones in the other shaders.
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
	// maybe go and check the refcount of the textures to possibly free them.
	s_render_inst->material_map.erase(a_material);
}

const RTexture BB::UploadTexture(const RCommandList a_list, const UploadTextureInfo& a_upload_info, const uint64_t a_transfer_fence_value)
{
	return s_render_inst->texture_manager.UploadTexture(a_list, a_upload_info, a_transfer_fence_value);
}

void BB::FreeTexture(const RTexture a_texture)
{
	return s_render_inst->texture_manager.FreeTexture(a_texture);
}

bool BB::ReadTexture(MemoryArena& a_arena, const RTexture a_texture, uint32_t& a_width, uint32_t& a_height, uint32_t& a_channels, void*& a_data)
{
	const GPUTextureManager::TextureSlot& texture_slot = s_render_inst->texture_manager.GetTextureSlot(a_texture);

	uint32_t byte_per_pixel;
	bool requires_formatting = false;
	switch (texture_slot.texture_info.format)
	{
	case IMAGE_FORMAT::RGBA16_UNORM:
	case IMAGE_FORMAT::RGBA16_SFLOAT:
		byte_per_pixel = 8;
		break;
	case IMAGE_FORMAT::RGBA8_SRGB:
		byte_per_pixel = 4;
		requires_formatting = true;
		break;
	case IMAGE_FORMAT::RGBA8_UNORM:
		byte_per_pixel = 4;
		break;
	case IMAGE_FORMAT::A8_UNORM:
		byte_per_pixel = 1;
		break;
	default:
		byte_per_pixel = 4;
		BB_ASSERT(false, "unknown IMAGE_FORMAT");
		break;
	}

	const size_t image_size =
		static_cast<size_t>(texture_slot.texture_info.width) *
		static_cast<size_t>(texture_slot.texture_info.height) *
		byte_per_pixel;

	GPUWaitIdle();

	CommandPool& cmd_pool = GetGraphicsCommandPool();
	const RCommandList cmd_list = cmd_pool.StartCommandList("gpu image to cpu");

	const IMAGE_LAYOUT previous_layout = texture_slot.texture_info.current_layout;

	{
		PipelineBarrierImageInfo image_write_transition;
		image_write_transition.src_mask = BARRIER_ACCESS_MASK::NONE;
		image_write_transition.dst_mask = BARRIER_ACCESS_MASK::TRANSFER_READ;
		image_write_transition.image = texture_slot.texture_info.image;
		image_write_transition.old_layout = previous_layout;
		image_write_transition.new_layout = IMAGE_LAYOUT::TRANSFER_SRC;
		image_write_transition.src_queue = QUEUE_TRANSITION::NO_TRANSITION;
		image_write_transition.dst_queue = QUEUE_TRANSITION::NO_TRANSITION;
		image_write_transition.layer_count = 1;
		image_write_transition.level_count = 1;
		image_write_transition.base_array_layer = 0;
		image_write_transition.base_mip_level = 0;
		image_write_transition.src_stage = BARRIER_PIPELINE_STAGE::TOP_OF_PIPELINE;
		image_write_transition.dst_stage = BARRIER_PIPELINE_STAGE::TRANSFER;

		PipelineBarrierInfo pipeline_info{};
		pipeline_info.image_info_count = 1;
		pipeline_info.image_infos = &image_write_transition;
		Vulkan::PipelineBarriers(cmd_list, pipeline_info);
	}

	// not sure if we want a global readback buffer thing.
	GPUBufferCreateInfo readback_info{};
	readback_info.name = "readback for screenshot";
	readback_info.host_writable = true;
	readback_info.type = BUFFER_TYPE::READBACK;
	readback_info.size = image_size;

	const GPUBuffer readback_buffer = Vulkan::CreateBuffer(readback_info);

	{
		RenderCopyImageToBufferInfo image_to_buffer;
		image_to_buffer.dst_buffer = readback_buffer;
		image_to_buffer.dst_offset = 0; // change this if i have a global readback buffer thing

		image_to_buffer.src_image = texture_slot.texture_info.image;
		image_to_buffer.src_image_info.size_x = texture_slot.texture_info.width;
		image_to_buffer.src_image_info.size_y = texture_slot.texture_info.height;
		image_to_buffer.src_image_info.size_z = 1;
		image_to_buffer.src_image_info.offset_x = 0;
		image_to_buffer.src_image_info.offset_y = 0;
		image_to_buffer.src_image_info.offset_z = 0;
		image_to_buffer.src_image_info.layout = IMAGE_LAYOUT::TRANSFER_SRC;
		image_to_buffer.src_image_info.mip_level = 0;
		image_to_buffer.src_image_info.layer_count = 1;
		image_to_buffer.src_image_info.base_array_layer = 0;

		Vulkan::CopyImageToBuffer(cmd_list, image_to_buffer);
	}

	//{
	//	PipelineBarrierImageInfo image_shader_transition;
	//	image_shader_transition.src_mask = BARRIER_ACCESS_MASK::TRANSFER_READ;
	//	image_shader_transition.dst_mask = BARRIER_ACCESS_MASK::SHADER_READ;
	//	image_shader_transition.image = texture_slot.texture_info.image;
	//	image_shader_transition.old_layout = IMAGE_LAYOUT::TRANSFER_SRC;
	//	image_shader_transition.new_layout = previous_layout;
	//	image_shader_transition.src_queue = QUEUE_TRANSITION::NO_TRANSITION;
	//	image_shader_transition.dst_queue = QUEUE_TRANSITION::NO_TRANSITION;
	//	image_shader_transition.layer_count = 1;
	//	image_shader_transition.level_count = 1;
	//	image_shader_transition.base_array_layer = 0;
	//	image_shader_transition.base_mip_level = 0;
	//	image_shader_transition.src_stage = BARRIER_PIPELINE_STAGE::TRANSFER;
	//	image_shader_transition.dst_stage = BARRIER_PIPELINE_STAGE::FRAGMENT_SHADER;

	//	PipelineBarrierInfo pipeline_info{};
	//	pipeline_info.image_info_count = 1;
	//	pipeline_info.image_infos = &image_shader_transition;
	//	Vulkan::PipelineBarriers(cmd_list, pipeline_info);
	//}

	cmd_pool.EndCommandList(cmd_list);
	ExecuteGraphicCommands(Slice(&cmd_pool, 1));

	GPUWaitIdle();

	void* readback_memory = Vulkan::MapBufferMemory(readback_buffer);

	a_data = ArenaAlloc(a_arena, image_size, alignof(size_t));
	memcpy(a_data, readback_memory, image_size);
	a_width = texture_slot.texture_info.width;
	a_height = texture_slot.texture_info.height;
	a_channels = byte_per_pixel;

	Vulkan::UnmapBufferMemory(readback_buffer);
	Vulkan::FreeBuffer(readback_buffer);

	return true;
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

void BB::DrawMesh(const RenderScene3DHandle a_scene, const MeshHandle a_mesh, const float4x4& a_transform, const uint32_t a_index_start, const uint32_t a_index_count, const MaterialHandle a_material)
{
	Scene3D& render_scene3d = *reinterpret_cast<Scene3D*>(a_scene.handle);
	render_scene3d.draw_list_data.mesh_draw_call[render_scene3d.draw_list_count].mesh = a_mesh;
	render_scene3d.draw_list_data.mesh_draw_call[render_scene3d.draw_list_count].material = a_material;
	render_scene3d.draw_list_data.mesh_draw_call[render_scene3d.draw_list_count].index_start = a_index_start;
	render_scene3d.draw_list_data.mesh_draw_call[render_scene3d.draw_list_count].index_count = a_index_count;
	render_scene3d.draw_list_data.transform[render_scene3d.draw_list_count].transform = a_transform;
	render_scene3d.draw_list_data.transform[render_scene3d.draw_list_count++].inverse = Float4x4Inverse(a_transform);
}

RTexture BB::GetWhiteTexture()
{
	return s_render_inst->white;
}

RTexture BB::GetBlackTexture()
{
	return s_render_inst->black;
}

RTexture BB::GetDebugTexture()
{
	return s_render_inst->texture_manager.GetDebugTexture();
}
