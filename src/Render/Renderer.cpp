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
	void Init(MemoryArena& a_arena, const RDescriptorLayout a_global_layout, const DescriptorAllocation& a_allocation)
	{
		m_graphics_texture_transitions.Init(a_arena, MAX_TEXTURES / 4);
		m_graphics_texture_transitions.resize(m_graphics_texture_transitions.capacity());

		m_views = ArenaAllocArr(a_arena, RImageView, MAX_TEXTURES);
		m_next_free = 0;
		m_lock = OSCreateRWLock();

		// setup the freelist before making the debug texture
		for (uint32_t i = m_next_free; i < MAX_TEXTURES - 1; i++)
		{
			m_views[i].index = i + 1;
		}
		m_views[MAX_TEXTURES - 1].index = UINT32_MAX;

		{	// create debug texture
			const uint32_t debug_purple = (209u << 0u) | (106u << 8u) | (255u << 16u) | (255u << 24u);
			CreateBasicColorImage(m_debug_image, m_debug_descriptor_index, "debug purple", debug_purple);
		}

		DescriptorWriteImageInfo image_write;
		image_write.descriptor_layout = a_global_layout;
		image_write.allocation = a_allocation;
		image_write.view = GetImageView(m_debug_descriptor_index);
		image_write.layout = IMAGE_LAYOUT::SHADER_READ_ONLY;
		image_write.binding = GLOBAL_BINDLESS_TEXTURES_BINDING;
		for (uint32_t i = 0; i < MAX_TEXTURES; i++)
		{
			image_write.descriptor_index = i;
			DescriptorWriteImage(image_write);
		}
	}

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
		write_info.layout = IMAGE_LAYOUT::SHADER_READ_ONLY;
		write_info.allocation = a_allocation;
		write_info.descriptor_layout = a_global_layout;

		DescriptorWriteImage(write_info);

		return RDescriptorIndex(descriptor_index);
	}

	void FreeImageView(const RDescriptorIndex a_descriptor_index, const RDescriptorLayout a_global_layout, const DescriptorAllocation& a_allocation)
	{
		OSAcquireSRWLockWrite(&m_lock);
		Vulkan::FreeViewImage(m_views[a_descriptor_index.handle]);
		m_views[a_descriptor_index.handle].index = m_next_free;
		m_next_free = m_views[a_descriptor_index.handle].index;
		OSReleaseSRWLockWrite(&m_lock);

		DescriptorWriteImageInfo write_info;
		write_info.binding = GLOBAL_BINDLESS_TEXTURES_BINDING;
		write_info.descriptor_index = a_descriptor_index.handle;
		write_info.view = GetImageView(m_debug_descriptor_index);
		write_info.layout = IMAGE_LAYOUT::SHADER_READ_ONLY;
		write_info.allocation = a_allocation;
		write_info.descriptor_layout = a_global_layout;

		DescriptorWriteImage(write_info);
	}

	const RImageView GetImageView(const RDescriptorIndex a_index) const
	{
		return m_views[a_index.handle];
	}

	void TransitionTextures(const RCommandList a_list);

	void AddGraphicsTransition(const PipelineBarrierImageInfo& a_transition_info)
	{
		const uint32_t index = m_transition_size.fetch_add(1);
		m_graphics_texture_transitions[index] = a_transition_info;
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

	RDescriptorIndex GetDebugImageViewDescriptorIndex() const { return m_debug_descriptor_index; }
	RImageView GetDebugImageView() const { return GetImageView(m_debug_descriptor_index); }

private:
	uint32_t m_next_free;
	RImageView* m_views;
	BBRWLock m_lock;

	std::atomic<uint32_t> m_transition_size;
	StaticArray<PipelineBarrierImageInfo> m_graphics_texture_transitions;
	
	// purple color
	RImage m_debug_image;
	RDescriptorIndex m_debug_descriptor_index;
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
		  transfer_queue(a_arena, QUEUE_TYPE::TRANSFER, "transfer queue", 8, 8)
	{}

	RenderIO render_io;

	RImage render_target_image;
	struct Frame
	{
		RImageView render_target_view;
		RDescriptorIndex render_target_descriptor;
		uint64_t graphics_queue_fence_value;
	};
	Frame* frames;
	bool debug;

	ShaderCompiler shader_compiler;

	RenderQueue graphics_queue;
	RenderQueue transfer_queue;

	GPUTextureManager texture_manager;
	struct BasicColorImage
	{
		RImage image;
		RDescriptorIndex index;
	};
	BasicColorImage white;
	BasicColorImage black;

	GPUBufferView cubemap_position;

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
		uint64_t size;
		std::atomic<uint64_t> used;
	} vertex_buffer;
	struct CPUVertexBuffer
	{
		GPUBuffer buffer;
		uint64_t size;
		std::atomic<uint64_t> used;
		void* start_mapped;
	} cpu_vertex_buffer;

	struct IndexBuffer
	{
		GPUBuffer buffer;
		uint64_t size;
		std::atomic<uint64_t> used;
	} index_buffer;
	struct CPUIndexBuffer
	{
		GPUBuffer buffer;
		uint64_t size;
		std::atomic<uint64_t> used;
		void* start_mapped;
	} cpu_index_buffer;

	StaticSlotmap<ShaderEffect, ShaderEffectHandle> shader_effects{};
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

	struct ImRenderData
	{
		RImage font_image;				 // 8
		RDescriptorIndex font_descriptor;// 12

		// Render buffers for main window
		uint32_t frame_index;            // 16
		ImRenderBuffer* frame_buffers;	 // 24
	};

	inline static ImRenderData* ImGetRenderData()
	{
		return ImGui::GetCurrentContext() ? reinterpret_cast<ImRenderData*>(ImGui::GetIO().BackendRendererUserData) : nullptr;
	}

	inline static void ImSetRenderState(const ImDrawData& a_draw_data, const RCommandList a_cmd_list, const uint32_t a_vert_pos, const ShaderObject* a_vertex_and_fragment, const RPipelineLayout a_pipeline_layout)
	{
		ImRenderData* bd = ImGetRenderData();

		{
			constexpr SHADER_STAGE IMGUI_SHADER_STAGES[2]{ SHADER_STAGE::VERTEX, SHADER_STAGE::FRAGMENT_PIXEL };
			Vulkan::BindShaders(a_cmd_list, _countof(IMGUI_SHADER_STAGES), IMGUI_SHADER_STAGES, a_vertex_and_fragment);
			Vulkan::SetFrontFace(a_cmd_list, true);
			Vulkan::SetCullMode(a_cmd_list, CULL_MODE::NONE);
		}

		// Setup scale and translation:
		// Our visible imgui space lies from draw_data->DisplayPps (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayPos is (0,0) for single viewport apps.
		{
			ShaderIndices2D shader_indices;
			shader_indices.vertex_buffer_offset = a_vert_pos;
			shader_indices.albedo_texture = bd->font_descriptor;
			shader_indices.rect_scale.x = 2.0f / a_draw_data.DisplaySize.x;
			shader_indices.rect_scale.y = 2.0f / a_draw_data.DisplaySize.y;
			shader_indices.translate.x = -1.0f - a_draw_data.DisplayPos.x * shader_indices.rect_scale.x;
			shader_indices.translate.y = -1.0f - a_draw_data.DisplayPos.y * shader_indices.rect_scale.y;

			Vulkan::SetPushConstants(a_cmd_list, a_pipeline_layout, 0, sizeof(shader_indices), &shader_indices);
		}
	}

	inline static void ImGrowFrameBufferGPUBuffers(ImRenderBuffer& a_rb, const size_t a_new_vertex_size, const size_t a_new_index_size)
	{
		// free I guess, lol can't do that now XDDD

		a_rb.vertex_buffer = AllocateFromWritableVertexBuffer(a_new_vertex_size);
		a_rb.index_buffer = AllocateFromWritableIndexBuffer(a_new_index_size);
	}


	static void ImRenderFrame(const RCommandList a_cmd_list, const RImageView render_target_view, const bool a_clear_image, const ShaderEffectHandle a_vertex, const ShaderEffectHandle a_fragment)
	{
		const ImDrawData& draw_data = *ImGui::GetDrawData();
		// Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
		int fb_width = static_cast<int>(draw_data.DisplaySize.x * draw_data.FramebufferScale.x);
		int fb_height = static_cast<int>(draw_data.DisplaySize.y * draw_data.FramebufferScale.y);
		if (fb_width <= 0 || fb_height <= 0)
			return;

		const ShaderEffect& vertex = s_render_inst->shader_effects[a_vertex];
		const ShaderEffect& fragment = s_render_inst->shader_effects[a_fragment];
		const ShaderObject shader_objects[2] = { vertex.shader_object, fragment.shader_object };
		const RPipelineLayout pipeline_layout = vertex.pipeline_layout;
		BB_ASSERT(vertex.pipeline_layout == fragment.pipeline_layout, "pipeline layout not the same for the imgui vertex and fragment shader");

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

		RenderingAttachmentColor color_attach{};
		color_attach.load_color = !a_clear_image;
		color_attach.store_color = true;
		color_attach.image_layout = IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL;
		color_attach.image_view = render_target_view;

		StartRenderingInfo imgui_pass_start{};
		imgui_pass_start.render_area_extent = { render_io.screen_width, render_io.screen_height };
		imgui_pass_start.render_area_offset = {};
		imgui_pass_start.color_attachments = Slice(&color_attach, 1);
		imgui_pass_start.depth_attachment = nullptr;
		Vulkan::StartRenderPass(a_cmd_list, imgui_pass_start);
		FixedArray<ColorBlendState, 1> blend_state;
		blend_state[0].blend_enable = true;
		blend_state[0].color_flags = 0xF;
		blend_state[0].color_blend_op = BLEND_OP::ADD;
		blend_state[0].src_blend = BLEND_MODE::FACTOR_SRC_ALPHA;
		blend_state[0].dst_blend = BLEND_MODE::FACTOR_ONE_MINUS_SRC_ALPHA;
		blend_state[0].alpha_blend_op = BLEND_OP::ADD;
		blend_state[0].src_alpha_blend = BLEND_MODE::FACTOR_ONE;
		blend_state[0].dst_alpha_blend = BLEND_MODE::FACTOR_ZERO;
		Vulkan::SetBlendMode(a_cmd_list, 0, blend_state.slice());

		// Setup desired CrossRenderer state
		ImSetRenderState(draw_data, a_cmd_list, 0, shader_objects, pipeline_layout);

		// Will project scissor/clipping rectangles into framebuffer space
		const ImVec2 clip_off = draw_data.DisplayPos;    // (0,0) unless using multi-viewports
		const ImVec2 clip_scale = draw_data.FramebufferScale; // (1,1) unless using retina display which are often (2,2)

		// Because we merged all buffers into a single one, we maintain our own offset into them
		uint32_t global_idx_offset = 0;
		uint32_t vertex_offset = static_cast<uint32_t>(rb.vertex_buffer.offset);

		Vulkan::BindIndexBuffer(a_cmd_list, rb.index_buffer.buffer, rb.index_buffer.offset);

		ImTextureID last_texture = bd->font_descriptor.handle;
		for (int n = 0; n < draw_data.CmdListsCount; n++)
		{
			const ImDrawList* cmd_list = draw_data.CmdLists[n];
			Vulkan::SetPushConstants(a_cmd_list, pipeline_layout, IM_OFFSETOF(ShaderIndices2D, vertex_buffer_offset), sizeof(vertex_offset), &vertex_offset);

			for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
			{
				const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
				if (pcmd->UserCallback != nullptr)
				{
					// User callback, registered via ImDrawList::AddCallback()
					// (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
					if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
						ImSetRenderState(draw_data, a_cmd_list, vertex_offset, shader_objects, pipeline_layout);
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
						Vulkan::SetPushConstants(a_cmd_list, pipeline_layout, IM_OFFSETOF(ShaderIndices2D, albedo_texture), sizeof(new_text), &new_text);
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
		ScissorInfo base_scissor{};
		base_scissor.offset.x = 0;
		base_scissor.offset.y = 0;
		base_scissor.extent = imgui_pass_start.render_area_extent;
		Vulkan::SetScissor(a_cmd_list, base_scissor);

		Vulkan::EndRenderPass(a_cmd_list);
	}

	static bool ImInit(MemoryArena& a_arena)
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImPlot::CreateContext();
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

		ImageCreateInfo font_info{};
		font_info.name = "imgui font";
		font_info.width = static_cast<uint32_t>(width);
		font_info.height = static_cast<uint32_t>(height);
		font_info.depth = 1;
		font_info.format = IMAGE_FORMAT::RGBA8_UNORM;
		font_info.usage = IMAGE_USAGE::TEXTURE;
		font_info.array_layers = 1;
		font_info.mip_levels = 1;
		font_info.use_optimal_tiling = true;
		font_info.type = IMAGE_TYPE::TYPE_2D;
		font_info.is_cube_map = false;
		bd->font_image = CreateImage(font_info);

		ImageViewCreateInfo view_info;
		view_info.name = "imgui font";
		view_info.image = bd->font_image;
		view_info.format = IMAGE_FORMAT::RGBA8_UNORM;
		view_info.base_array_layer = 0;
		view_info.array_layers = 1;
		view_info.mip_levels = 1;
		view_info.base_mip_level = 0;
		view_info.type = IMAGE_VIEW_TYPE::TYPE_2D;
		view_info.aspects = IMAGE_ASPECT::COLOR;
		bd->font_descriptor = CreateImageView(view_info);

		WriteImageInfo write_info{};
		write_info.image = bd->font_image;
		write_info.format = IMAGE_FORMAT::RGBA8_UNORM;
		write_info.extent = { font_info.width, font_info.height };
		write_info.pixels = pixels;
		write_info.set_shader_visible = true;
		write_info.layer_count = 1;
		write_info.base_array_layer = 0;
		WriteTexture(write_info);

		io.Fonts->SetTexID(bd->font_descriptor.handle);

		return bd->font_descriptor.IsValid() && bd->font_image.IsValid();
	}

	inline static void ImShutdown()
	{
		ImRenderData* bd = ImGetRenderData();
		BB_ASSERT(bd != nullptr, "No renderer backend to shutdown, or already shutdown?");
		ImGuiIO& io = ImGui::GetIO();

		//delete my things here.
		FreeImage(bd->font_image);	
		bd->font_image = RImage();

		FreeImageView(bd->font_descriptor);
		bd->font_descriptor = RDescriptorIndex();

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

void GPUTextureManager::TransitionTextures(const RCommandList a_list)
{
	if (m_transition_size.load() == 0)
		return;
	
	PipelineBarrierInfo barrier_info{};
	barrier_info.image_infos = m_graphics_texture_transitions.data();
	barrier_info.image_info_count = m_graphics_texture_transitions.size();
	Vulkan::PipelineBarriers(a_list, barrier_info);
	m_transition_size.store(0);
};

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
		s_render_inst->vertex_buffer.size = static_cast<uint32_t>(vertex_buffer.size);
		s_render_inst->vertex_buffer.used = 0;

		vertex_buffer.host_writable = true;
		s_render_inst->cpu_vertex_buffer.buffer = Vulkan::CreateBuffer(vertex_buffer);
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
		s_render_inst->index_buffer.size = static_cast<uint32_t>(index_buffer.size);
		s_render_inst->index_buffer.used = 0;

		index_buffer.host_writable = true;
		s_render_inst->cpu_index_buffer.buffer = Vulkan::CreateBuffer(index_buffer);
		s_render_inst->cpu_index_buffer.size = static_cast<uint32_t>(index_buffer.size);
		s_render_inst->cpu_index_buffer.used = 0;
		s_render_inst->cpu_index_buffer.start_mapped = Vulkan::MapBufferMemory(s_render_inst->cpu_index_buffer.buffer);
	}

	// setup cube positions
	{
		CreateMeshInfo cube_info{};
		cube_info.positions = ConstSlice<float3>(SKYBOX_VERTICES, _countof(SKYBOX_VERTICES));
		const Mesh cube_map = CreateMesh(cube_info);
		s_render_inst->global_buffer.data.cube_vertexpos_vertex_buffer_pos = static_cast<uint32_t>(cube_map.vertex_position_offset);
	}

	{	//initialize texture system
		s_render_inst->texture_manager.Init(a_arena, s_render_inst->global_descriptor_set, s_render_inst->global_descriptor_allocation);

		//some basic colors
		const uint32_t white = UINT32_MAX;
		CreateBasicColorImage(s_render_inst->white.image, s_render_inst->white.index, "white", white);
		const uint32_t black = 0x000000FF;
		CreateBasicColorImage(s_render_inst->black.image, s_render_inst->black.index, "black", black);
	}

	IMGUI_IMPL::ImInit(a_arena);

	{
		ImageCreateInfo render_target_info;
		render_target_info.name = "image before transfer to swapchain";
		render_target_info.width = s_render_inst->render_io.screen_width;
		render_target_info.height = s_render_inst->render_io.screen_height;
		render_target_info.depth = 1;
		render_target_info.array_layers = static_cast<uint16_t>(s_render_inst->render_io.frame_count);
		render_target_info.mip_levels = 1;
		render_target_info.type = IMAGE_TYPE::TYPE_2D;
		render_target_info.use_optimal_tiling = true;
		render_target_info.is_cube_map = false;
		render_target_info.format = IMAGE_FORMAT::RGBA16_SFLOAT;
		render_target_info.usage = IMAGE_USAGE::SWAPCHAIN_COPY_IMG;

		s_render_inst->render_target_image = CreateImage(render_target_info);

		s_render_inst->frames = ArenaAllocArr(a_arena, RenderInterface_inst::Frame, s_render_inst->render_io.frame_count);
		for (size_t i = 0; i < s_render_inst->render_io.frame_count; i++)
		{
			ImageViewCreateInfo view_info;
			view_info.name = "image before transfer to swapchain";
			view_info.image = s_render_inst->render_target_image;
			view_info.array_layers = 1;
			view_info.base_array_layer = static_cast<uint16_t>(i);
			view_info.mip_levels = 1;
			view_info.base_mip_level = 0;
			view_info.aspects = IMAGE_ASPECT::COLOR;
			view_info.type = IMAGE_VIEW_TYPE::TYPE_2D;
			view_info.format = IMAGE_FORMAT::RGBA16_SFLOAT;

			s_render_inst->frames[i].render_target_descriptor = CreateImageView(view_info);
			s_render_inst->frames[i].render_target_view = GetImageView(s_render_inst->frames[i].render_target_descriptor);
		}
	}

	return true;
}

bool BB::DestroyRenderer()
{
	IMGUI_IMPL::ImShutdown();
	BB_UNIMPLEMENTED("BB::DestroyRenderer");
	// delete all vulkan objects
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

	FreeImage(s_render_inst->render_target_image);

	ImageCreateInfo render_target_info;
	render_target_info.name = "image before transfer to swapchain";
	render_target_info.width = s_render_inst->render_io.screen_width;
	render_target_info.height = s_render_inst->render_io.screen_height;
	render_target_info.depth = 1;
	render_target_info.array_layers = static_cast<uint16_t>(s_render_inst->render_io.frame_count);
	render_target_info.mip_levels = 1;
	render_target_info.type = IMAGE_TYPE::TYPE_2D;
	render_target_info.use_optimal_tiling = true;
	render_target_info.is_cube_map = false;
	render_target_info.format = IMAGE_FORMAT::RGBA16_SFLOAT;
	render_target_info.usage = IMAGE_USAGE::SWAPCHAIN_COPY_IMG;

	s_render_inst->render_target_image = CreateImage(render_target_info);

	for (size_t i = 0; i < s_render_inst->render_io.frame_count; i++)
	{
		FreeImageView(s_render_inst->frames[i].render_target_descriptor);

		ImageViewCreateInfo view_info;
		view_info.name = "image before transfer to swapchain";
		view_info.image = s_render_inst->render_target_image;
		view_info.array_layers = 1;
		view_info.base_array_layer = 0;
		view_info.mip_levels = 1;
		view_info.base_mip_level = 0;
		view_info.aspects = IMAGE_ASPECT::COLOR;
		view_info.type = IMAGE_VIEW_TYPE::TYPE_2D;
		view_info.format = IMAGE_FORMAT::RGBA16_SFLOAT;

		s_render_inst->frames[i].render_target_descriptor = CreateImageView(view_info);
		s_render_inst->frames[i].render_target_view = GetImageView(s_render_inst->frames[i].render_target_descriptor);
	}

	s_render_inst->global_buffer.data.swapchain_resolution = uint2(a_width, a_height);

	// also reset the render debug state
	s_render_inst->render_io.frame_started = false;
	s_render_inst->render_io.frame_ended = false;
	s_render_inst->render_io.resizing_request = false;
}

GPUDeviceInfo BB::GetGPUInfo(MemoryArena& a_arena)
{
	return Vulkan::GetGPUDeviceInfo(a_arena);
}

void BB::RenderStartFrame(const RCommandList a_list, const RenderStartFrameInfo& a_info, uint32_t& a_out_back_buffer_index)
{
	// check if we need to resize
	if (s_render_inst->render_io.resizing_request)
	{
		int x, y;
		OSGetWindowSize(s_render_inst->render_io.window_handle, x, y);
		ResizeRendererSwapchain(static_cast<uint32_t>(x), static_cast<uint32_t>(y));
	}

	BB_ASSERT(s_render_inst->render_io.frame_started == false, "did not call RenderEndFrame before a new RenderStartFrame");
	s_render_inst->render_io.frame_started = true;

	const uint32_t frame_index = s_render_inst->render_io.frame_index;
	const RenderInterface_inst::Frame& cur_frame = s_render_inst->frames[frame_index];

	s_render_inst->graphics_queue.WaitFenceValue(cur_frame.graphics_queue_fence_value);

	{
		PipelineBarrierImageInfo image_transitions[1]{};
		image_transitions[0].src_mask = BARRIER_ACCESS_MASK::NONE;
		image_transitions[0].dst_mask = BARRIER_ACCESS_MASK::COLOR_ATTACHMENT_WRITE;
		image_transitions[0].image = s_render_inst->render_target_image;
		image_transitions[0].old_layout = IMAGE_LAYOUT::UNDEFINED;
		image_transitions[0].new_layout = IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL;
		image_transitions[0].layer_count = 1;
		image_transitions[0].level_count = 1;
		image_transitions[0].base_array_layer = frame_index;
		image_transitions[0].base_mip_level = 0;
		image_transitions[0].src_stage = BARRIER_PIPELINE_STAGE::TOP_OF_PIPELINE;
		image_transitions[0].dst_stage = BARRIER_PIPELINE_STAGE::COLOR_ATTACH_OUTPUT;
		image_transitions[0].aspects = IMAGE_ASPECT::COLOR;

		PipelineBarrierInfo pipeline_info{};
		pipeline_info.image_info_count = _countof(image_transitions);
		pipeline_info.image_infos = image_transitions;
		Vulkan::PipelineBarriers(a_list, pipeline_info);
	}

	{
		s_render_inst->global_buffer.data.frame_count += 1;
		s_render_inst->global_buffer.data.mouse_pos = a_info.mouse_pos;
		s_render_inst->global_buffer.data.frame_index = s_render_inst->render_io.frame_index;
		s_render_inst->global_buffer.data.delta_time = a_info.delta_time;
		s_render_inst->global_buffer.data.total_time += a_info.delta_time;
		memcpy(s_render_inst->global_buffer.mapped,
			&s_render_inst->global_buffer.data,
			sizeof(s_render_inst->global_buffer.data));
	}

	IMGUI_IMPL::ImNewFrame();
	ImGui::NewFrame();
	ImguiDisplayRenderer();

	a_out_back_buffer_index = frame_index;
}

void BB::RenderEndFrame(const RCommandList a_list, const ShaderEffectHandle a_imgui_vertex, const ShaderEffectHandle a_imgui_fragment, const uint32_t a_back_buffer_index, bool a_skip)
{
	BB_ASSERT(s_render_inst->render_io.frame_started == true, "did not call RenderStartFrame before a RenderEndFrame");

	if (a_skip || s_render_inst->render_io.resizing_request)
	{
		ImGui::EndFrame();
		return;
	}
	const uint32_t frame_index = a_back_buffer_index;
	const RenderInterface_inst::Frame& cur_frame = s_render_inst->frames[frame_index];

	ImGui::Render();
	IMGUI_IMPL::ImRenderFrame(a_list, cur_frame.render_target_view, true, a_imgui_vertex, a_imgui_fragment);
	ImGui::EndFrame();

	{
		PipelineBarrierImageInfo image_transitions[1]{};
		image_transitions[0].src_mask = BARRIER_ACCESS_MASK::COLOR_ATTACHMENT_WRITE;
		image_transitions[0].dst_mask = BARRIER_ACCESS_MASK::TRANSFER_READ;
		image_transitions[0].image = s_render_inst->render_target_image;
		image_transitions[0].old_layout = IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL;
		image_transitions[0].new_layout = IMAGE_LAYOUT::TRANSFER_SRC;
		image_transitions[0].layer_count = 1;
		image_transitions[0].level_count = 1;
		image_transitions[0].base_array_layer = a_back_buffer_index;
		image_transitions[0].base_mip_level = 0;
		image_transitions[0].src_stage = BARRIER_PIPELINE_STAGE::COLOR_ATTACH_OUTPUT;
		image_transitions[0].dst_stage = BARRIER_PIPELINE_STAGE::TRANSFER;
		image_transitions[0].aspects = IMAGE_ASPECT::COLOR;

		PipelineBarrierInfo pipeline_info{};
		pipeline_info.image_info_count = _countof(image_transitions);
		pipeline_info.image_infos = image_transitions;
		Vulkan::PipelineBarriers(a_list, pipeline_info);
	}

	s_render_inst->texture_manager.TransitionTextures(a_list);

	const int2 swapchain_size(static_cast<int>(s_render_inst->render_io.screen_width), static_cast<int>(s_render_inst->render_io.screen_height));

	const PRESENT_IMAGE_RESULT result = Vulkan::UploadImageToSwapchain(a_list, s_render_inst->render_target_image, swapchain_size, swapchain_size, a_back_buffer_index);
	if (result == PRESENT_IMAGE_RESULT::SWAPCHAIN_OUT_OF_DATE)
		s_render_inst->render_io.resizing_request = true;
	s_render_inst->render_io.frame_ended = true;
}

void BB::StartRenderPass(const RCommandList a_list, const StartRenderingInfo& a_render_info)
{
	Vulkan::StartRenderPass(a_list, a_render_info);
}

void BB::EndRenderPass(const RCommandList a_list)
{
	Vulkan::EndRenderPass(a_list);
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

	// set the global index buffer
	Vulkan::BindIndexBuffer(a_list, s_render_inst->index_buffer.buffer, 0);

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

bool BB::PresentFrame(const BB::Slice<CommandPool> a_cmd_pools, const RFence* a_signal_fences, const uint64_t* a_signal_values, const uint32_t a_signal_count, uint64_t& a_out_present_fence_value)
{
	if (s_render_inst->render_io.resizing_request)
	{
		s_render_inst->graphics_queue.ReturnPools(a_cmd_pools);
		return false;
	}

	BB_ASSERT(s_render_inst->render_io.frame_started == true, "did not call RenderStartFrame before a presenting");
	BB_ASSERT(s_render_inst->render_io.frame_ended == true, "did not call RenderEndFrame before a presenting");

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


	s_render_inst->graphics_queue.ReturnPools(a_cmd_pools);
	const PRESENT_IMAGE_RESULT result = s_render_inst->graphics_queue.ExecutePresentCommands(lists, list_count, a_signal_fences, a_signal_values, a_signal_count, nullptr, nullptr, 0, s_render_inst->render_io.frame_index, a_out_present_fence_value);
	s_render_inst->render_io.frame_index = (s_render_inst->render_io.frame_index + 1) % s_render_inst->render_io.frame_count;
	s_render_inst->frames[s_render_inst->render_io.frame_index].graphics_queue_fence_value = a_out_present_fence_value;

	s_render_inst->render_io.frame_ended = false;
	s_render_inst->render_io.frame_started = false;

	if (result == PRESENT_IMAGE_RESULT::SWAPCHAIN_OUT_OF_DATE)
		s_render_inst->render_io.resizing_request = true;

	return true;
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

void BB::CopyBuffer(const RCommandList a_list, const RenderCopyBuffer& a_copy_buffer)
{
	Vulkan::CopyBuffer(a_list, a_copy_buffer);
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

void BB::WaitFence(const RFence a_fence, const uint64_t a_fence_value)
{
	Vulkan::WaitFence(a_fence, a_fence_value);
}

void BB::WaitFences(const RFence* a_fences, const uint64_t* a_fence_values, const uint32_t a_fence_count)
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

RDescriptorIndex BB::GetWhiteTexture()
{
	return s_render_inst->white.index;
}

RDescriptorIndex BB::GetBlackTexture()
{
	return s_render_inst->black.index;
}

RDescriptorIndex BB::GetDebugTexture()
{
	return s_render_inst->texture_manager.GetDebugImageViewDescriptorIndex();
}

RDescriptorLayout BB::GetStaticSamplerDescriptorLayout()
{
	return s_render_inst->static_sampler_descriptor_set;
}

RDescriptorLayout BB::GetGlobalDescriptorLayout()
{
	return s_render_inst->global_descriptor_set;
}
