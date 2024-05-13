#include "Renderer.hpp"
#include "VulkanRenderer.hpp"

#include "Storage/Slotmap.h"
#include "Storage/Queue.hpp"
#include "Program.h"

#include "ShaderCompiler.h"

#include "Math.inl"

#include "imgui.h"

#include "BBThreadScheduler.hpp"

#include "GPUBuffers.hpp"

using namespace BB;

constexpr float SKYBOX_VERTICES[] = {
	-1.0f,-1.0f,-1.0f,  // -X side
	-1.0f,-1.0f, 1.0f,
	-1.0f, 1.0f, 1.0f,
	-1.0f, 1.0f, 1.0f,
	-1.0f, 1.0f,-1.0f,
	-1.0f,-1.0f,-1.0f,

	-1.0f,-1.0f,-1.0f,  // -Z side
	 1.0f, 1.0f,-1.0f,
	 1.0f,-1.0f,-1.0f,
	-1.0f,-1.0f,-1.0f,
	-1.0f, 1.0f,-1.0f,
	 1.0f, 1.0f,-1.0f,

	-1.0f,-1.0f,-1.0f,  // -Y side
	 1.0f,-1.0f,-1.0f,
	 1.0f,-1.0f, 1.0f,
	-1.0f,-1.0f,-1.0f,
	 1.0f,-1.0f, 1.0f,
	-1.0f,-1.0f, 1.0f,

	-1.0f, 1.0f,-1.0f,  // +Y side
	-1.0f, 1.0f, 1.0f,
	 1.0f, 1.0f, 1.0f,
	-1.0f, 1.0f,-1.0f,
	 1.0f, 1.0f, 1.0f,
	 1.0f, 1.0f,-1.0f,

	 1.0f, 1.0f,-1.0f,  // +X side
	 1.0f, 1.0f, 1.0f,
	 1.0f,-1.0f, 1.0f,
	 1.0f,-1.0f, 1.0f,
	 1.0f,-1.0f,-1.0f,
	 1.0f, 1.0f,-1.0f,

	-1.0f, 1.0f, 1.0f,  // +Z side
	-1.0f,-1.0f, 1.0f,
	 1.0f, 1.0f, 1.0f,
	-1.0f,-1.0f, 1.0f,
	 1.0f,-1.0f, 1.0f,
	 1.0f, 1.0f, 1.0f,
};

struct RenderFence
{
	uint64_t next_fence_value;
	uint64_t last_complete_value;
	RFence fence;
};
constexpr uint32_t MAX_MESH_UPLOAD_QUEUE = 128;
constexpr uint32_t MAX_TEXTURE_UPLOAD_QUEUE = 256;

constexpr uint32_t MAX_TEXTURES = 1024;
constexpr const char* DEBUG_TEXTURE_NAME = "debug texture";

constexpr IMAGE_FORMAT RENDER_TARGET_IMAGE_FORMAT = IMAGE_FORMAT::RGBA8_SRGB; // due to screenshots this is now RGBA8_SGRB, should be RGBA16_SFLOAT

constexpr IMAGE_FORMAT SCREENSHOT_IMAGE_FORMAT = IMAGE_FORMAT::RGBA8_SRGB; // check the variable below here before you change shit k thnx
constexpr size_t SCREENSHOT_IMAGE_PIXEL_BYTE_SIZE = 4;	// check the variable above here before you change shit k thnx

struct TextureInfo
{
	RImage image;				// 8
	RImageView view;			// 16

	uint32_t width;				// 20
	uint32_t height;			// 24
	uint32_t array_layers;		// 28
	uint32_t mip_levels;		// 32
	IMAGE_FORMAT format;		// 36
	IMAGE_LAYOUT current_layout;// 40

	// debug extra, not required for anything
	IMAGE_USAGE usage;			// 44
};

class GPUTextureManager
{
public:
	struct TextureSlot
	{
		const char* name;			// 8
		TextureInfo texture_info;	// 44
		uint32_t next_free;			// 48
	};

	void Init(MemoryArena& a_arena, const RCommandList a_list);

	const RTexture SetTextureSlot(const TextureInfo& a_texture_info, const char* a_name);

	void TransitionTextures(const RCommandList a_list);

	void AddGraphicsTransition(const PipelineBarrierImageInfo& a_transition_info)
	{
		OSAcquireSRWLockWrite(&m_lock);
		m_graphics_texture_transitions.emplace_back(a_transition_info);
		OSReleaseSRWLockWrite(&m_lock);
	}

	void AddGraphicsTransition(const PipelineBarrierImageInfo& a_transition_info, const WriteDescriptorData& a_write_descriptor)
	{
		OSAcquireSRWLockWrite(&m_lock);
		m_graphics_texture_transitions.emplace_back(a_transition_info);
		m_descriptor_writes.emplace_back(a_write_descriptor);
		OSReleaseSRWLockWrite(&m_lock);
	}

	void FreeTexture(const RTexture a_texture);

	TextureSlot& GetTextureSlot(const RTexture a_texture)
	{
		return m_textures[a_texture.handle];
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
						ImGui::TextUnformatted("UNNAMED! This might be an error");

					const ImVec2 image_size = { 160, 160 };
					for (uint32_t array_layer = 0; array_layer < slot.texture_info.array_layers; array_layer++)
					{
						ImGui::Text("array layer: %u", array_layer);
						for (uint32_t mip_level = 1; mip_level < slot.texture_info.mip_levels + 1; mip_level++)
						{
							ImGui::Text("mip level: %u", mip_level);
							ImGui::Image(i, image_size);
						}
					}

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
	
	StaticArray<WriteDescriptorData> m_descriptor_writes;
	StaticArray<PipelineBarrierImageInfo> m_graphics_texture_transitions;
	
	// purple color
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

//THREAD SAFE: TRUE
class RenderQueue
{
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
	ShaderObjectCreateInfo create_info;
#endif // _ENABLE_REBUILD_SHADERS
};

struct Material
{
	struct Shader
	{
		RPipelineLayout pipeline_layout;
		
		ShaderEffectHandle shader_effects[UNIQUE_SHADER_STAGE_COUNT];
		uint32_t shader_effect_count;
	} shader;

	const char* name;
};

constexpr uint32_t BACK_BUFFER_MAX = 3;

struct UploadDataMesh
{
	MeshHandle mesh_index;
	RenderCopyBufferRegion vertex_region;
	RenderCopyBufferRegion index_region;
};

enum class UPLOAD_TEXTURE_TYPE
{
	WRITE,
	READ
};

struct UploadDataTexture
{
	RTexture texture_index;
	UPLOAD_TEXTURE_TYPE upload_type;
	union 
	{
		RenderCopyBufferToImageInfo write_info;
		RenderCopyImageToBufferInfo read_info;
	};
	IMAGE_LAYOUT start_layout;
	IMAGE_LAYOUT end_layout;
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

	GPUUploadRingAllocator frame_upload_allocator;
	RenderQueue graphics_queue;
	RenderQueue transfer_queue;

	struct AssetUploader
	{
		GPUUploadRingAllocator gpu_allocator;
		RFence fence;
		std::atomic<uint64_t> next_fence_value;
		MPSCQueue<UploadDataMesh> upload_meshes;
		MPSCQueue<UploadDataTexture> upload_textures;
	};
	AssetUploader asset_uploader;

	GPUTextureManager texture_manager;

	GPUBufferView cubemap_position;
	MaterialHandle standard_3d_material;
	RTexture white;
	RTexture black;

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
		uint32_t size;
		std::atomic<uint32_t> used;
	} vertex_buffer;
	struct CPUVertexBuffer
	{
		GPUBuffer buffer;
		uint32_t size;
		std::atomic<uint32_t> used;
		void* start_mapped;
	} cpu_vertex_buffer;

	struct IndexBuffer
	{
		GPUBuffer buffer;
		uint32_t size;
		std::atomic<uint32_t> used;
	} index_buffer;
	struct CPUIndexBuffer
	{
		GPUBuffer buffer;
		uint32_t size;
		std::atomic<uint32_t> used;
		void* start_mapped;
	} cpu_index_buffer;

	StaticSlotmap<Mesh, MeshHandle> mesh_map{};
	StaticSlotmap<ShaderEffect, ShaderEffectHandle> shader_effects{};
	StaticSlotmap<Material, MaterialHandle> material_map{};
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
		RTexture font_image;			// 4

		// Render buffers for main window
		uint32_t frame_index;           // 8
		ImRenderBuffer* frame_buffers;	// 16
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
			shader_indices.albedo_texture = bd->font_image.handle;
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

		// Setup desired CrossRenderer state
		ImSetRenderState(draw_data, a_cmd_list, 0, shader_objects, pipeline_layout);

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

		CreateTextureInfo font_info{};
		font_info.name = "imgui font";
		font_info.width = static_cast<uint32_t>(width);
		font_info.height = static_cast<uint32_t>(height);
		font_info.format = IMAGE_FORMAT::RGBA8_UNORM;
		font_info.usage = IMAGE_USAGE::TEXTURE;
		font_info.array_layers = 1;
		bd->font_image = CreateTexture(font_info);
		WriteTextureInfo write_info{};
		write_info.extent = { font_info.width, font_info.height };
		write_info.pixels = pixels;
		write_info.set_shader_visible = true;
		write_info.layer_count = 1;
		write_info.base_array_layer = 0;
		WriteTexture(bd->font_image, write_info);

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
	}
	ImGui::End();
}

GPUBufferView BB::AllocateFromVertexBuffer(const size_t a_size_in_bytes)
{
	GPUBufferView view;
	view.buffer = s_render_inst->vertex_buffer.buffer;
	view.size = a_size_in_bytes;
	view.offset = s_render_inst->vertex_buffer.used.fetch_add(static_cast<uint32_t>(a_size_in_bytes));

	BB_ASSERT(s_render_inst->vertex_buffer.size > view.offset + a_size_in_bytes, "out of vertex buffer space!");

	return view;
}

GPUBufferView BB::AllocateFromIndexBuffer(const size_t a_size_in_bytes)
{
	GPUBufferView view;
	view.buffer = s_render_inst->index_buffer.buffer;
	view.size = a_size_in_bytes;
	view.offset = s_render_inst->index_buffer.used.fetch_add(static_cast<uint32_t>(a_size_in_bytes));

	BB_ASSERT(s_render_inst->index_buffer.size > view.offset + a_size_in_bytes, "out of index buffer space!");

	return view;
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

void GPUTextureManager::Init(MemoryArena& a_arena, const RCommandList a_list)
{
	m_descriptor_writes.Init(a_arena, MAX_TEXTURES / 4);
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
		image_view_info.type = IMAGE_VIEW_TYPE::TYPE_2D;
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
		UploadBuffer upload_buffer = s_render_inst->frame_upload_allocator.AllocateUploadMemory(sizeof(debug_purple), s_render_inst->asset_uploader.next_fence_value.load());
		upload_buffer.SafeMemcpy(0, &debug_purple, sizeof(debug_purple));

		RenderCopyBufferToImageInfo buffer_to_image;
		buffer_to_image.src_buffer = upload_buffer.buffer;
		buffer_to_image.src_offset = static_cast<uint32_t>(upload_buffer.base_offset);

		buffer_to_image.dst_image = m_debug_texture.image;
		buffer_to_image.dst_extent = uint3(1u, 1u, 1u);
		buffer_to_image.dst_image_info.offset_x = 0;
		buffer_to_image.dst_image_info.offset_y = 0;
		buffer_to_image.dst_image_info.offset_z = 0;
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
			write_desc[i].binding = GLOBAL_BINDLESS_TEXTURES_BINDING;
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

const RTexture GPUTextureManager::SetTextureSlot(const TextureInfo& a_texture_info, const char* a_name)
{
	OSAcquireSRWLockWrite(&m_lock);
	const RTexture texture_slot = RTexture(m_next_free);
	TextureSlot& slot = m_textures[texture_slot.handle];
	m_next_free = slot.next_free;
	OSReleaseSRWLockWrite(&m_lock);
	slot.name = a_name;
	slot.texture_info = a_texture_info;

	WriteDescriptorData write_desc{};
	write_desc.binding = GLOBAL_BINDLESS_TEXTURES_BINDING;
	write_desc.descriptor_index = texture_slot.handle; // handle is also the descriptor index
	write_desc.type = DESCRIPTOR_TYPE::IMAGE;
	write_desc.image_view.view = slot.texture_info.view;
	write_desc.image_view.layout = IMAGE_LAYOUT::UNDEFINED;

	WriteDescriptorInfos image_write_infos;
	image_write_infos.allocation = s_render_inst->global_descriptor_allocation;
	image_write_infos.descriptor_layout = s_render_inst->global_descriptor_set;
	image_write_infos.data = Slice(&write_desc, 1);

	Vulkan::WriteDescriptors(image_write_infos);

	slot.texture_info.current_layout = IMAGE_LAYOUT::UNDEFINED;
	return texture_slot;
}

void GPUTextureManager::TransitionTextures(const RCommandList a_list)
{
	if (m_graphics_texture_transitions.size() == 0 && m_descriptor_writes.size() == 0)
		return;

	OSAcquireSRWLockWrite(&m_lock);
	if (m_graphics_texture_transitions.size())
	{
		PipelineBarrierInfo barrier_info{};
		barrier_info.image_infos = m_graphics_texture_transitions.data();
		barrier_info.image_info_count = m_graphics_texture_transitions.size();
		Vulkan::PipelineBarriers(a_list, barrier_info);
		m_graphics_texture_transitions.clear();
	}

	if (m_descriptor_writes.size())
	{
		WriteDescriptorInfos image_write_infos{};
		image_write_infos.allocation = s_render_inst->global_descriptor_allocation;
		image_write_infos.descriptor_layout = s_render_inst->global_descriptor_set;
		image_write_infos.data = Slice(m_descriptor_writes.data(), m_descriptor_writes.size());
		Vulkan::WriteDescriptors(image_write_infos);
		m_descriptor_writes.clear();
	}

	OSReleaseSRWLockWrite(&m_lock);
};

static size_t GetByteSizeOfImageFormat(const IMAGE_FORMAT a_format)
{
	switch (a_format)
	{
	case IMAGE_FORMAT::RGBA16_UNORM:
	case IMAGE_FORMAT::RGBA16_SFLOAT:
		return 8;
	case IMAGE_FORMAT::RGBA8_SRGB:
	case IMAGE_FORMAT::RGBA8_UNORM:
		return 4;
	case IMAGE_FORMAT::RGB8_SRGB:
		return 3;
	case IMAGE_FORMAT::A8_UNORM:
		return 1;
	default:
		BB_ASSERT(false, "Unsupported bit_count for upload image");
		return 4;
	}
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
	s_render_inst->shader_effects.Init(a_arena, 64);
	s_render_inst->material_map.Init(a_arena, 256);

	s_render_inst->frame_upload_allocator.Init(a_arena, a_render_create_info.frame_upload_buffer_size, s_render_inst->graphics_queue.GetFence().fence, "frame upload allocator");

	

	{	// do asset upload allocator here
		s_render_inst->asset_uploader.upload_meshes.Init(a_arena, MAX_MESH_UPLOAD_QUEUE);
		s_render_inst->asset_uploader.upload_textures.Init(a_arena, MAX_TEXTURE_UPLOAD_QUEUE);
		s_render_inst->asset_uploader.gpu_allocator.Init(a_arena,
			a_render_create_info.asset_upload_buffer_size, 
			s_render_inst->asset_uploader.fence, 
			"asset upload buffer");
		s_render_inst->asset_uploader.fence = Vulkan::CreateFence(0, "asset upload fence");
		s_render_inst->asset_uploader.next_fence_value = 1;
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
			DescriptorBindingInfo descriptor_bindings[4];
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

			s_render_inst->global_descriptor_set = Vulkan::CreateDescriptorLayout(a_arena, Slice(descriptor_bindings, _countof(descriptor_bindings)));
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

		WriteDescriptorData global_descriptor_write[3]{};
		global_descriptor_write[0].binding = GLOBAL_VERTEX_BUFFER_BINDING;
		global_descriptor_write[0].descriptor_index = 0;
		global_descriptor_write[0].type = DESCRIPTOR_TYPE::READONLY_BUFFER;
		global_descriptor_write[0].buffer_view = view;

		global_descriptor_write[1].binding = GLOBAL_CPU_VERTEX_BUFFER_BINDING;
		global_descriptor_write[1].descriptor_index = 0;
		global_descriptor_write[1].type = DESCRIPTOR_TYPE::READONLY_BUFFER;
		global_descriptor_write[1].buffer_view = cpu_view;

		GPUBufferView global_view;
		cpu_view.buffer = s_render_inst->global_buffer.buffer;
		cpu_view.offset = 0;
		cpu_view.size = sizeof(s_render_inst->global_buffer.data);

		global_descriptor_write[2].binding = GLOBAL_BUFFER_BINDING;
		global_descriptor_write[2].descriptor_index = 0;
		global_descriptor_write[2].type = DESCRIPTOR_TYPE::READONLY_CONSTANT;
		global_descriptor_write[2].buffer_view = cpu_view;

		WriteDescriptorInfos global_descriptor_info;
		global_descriptor_info.allocation = s_render_inst->global_descriptor_allocation;
		global_descriptor_info.descriptor_layout = s_render_inst->global_descriptor_set;
		global_descriptor_info.data = Slice(global_descriptor_write, _countof(global_descriptor_write));

		Vulkan::WriteDescriptors(global_descriptor_info);
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

	{	//initialize texture system
		s_render_inst->texture_manager.Init(a_arena, list);

		//some basic colors
		const uint32_t white = UINT32_MAX;
		const uint32_t black = 0x000000FF;
		
		CreateTextureInfo image_info;
		image_info.name = "white";
		image_info.width = 1;
		image_info.height = 1;
		image_info.array_layers = 1;
		image_info.format = IMAGE_FORMAT::RGBA8_SRGB;
		image_info.usage = IMAGE_USAGE::TEXTURE;
		WriteTextureInfo write_info{};
		write_info.extent = { image_info.width, image_info.height };
		write_info.set_shader_visible = true;
		write_info.pixels = &white;
		write_info.layer_count = 1;
		write_info.base_array_layer = 0;

		s_render_inst->white = CreateTexture(image_info);
		WriteTexture(s_render_inst->white, write_info);

		image_info.name = "black";
		s_render_inst->black = CreateTexture(image_info);
		write_info.pixels = &black;
		WriteTexture(s_render_inst->black, write_info);
	}

	{
		CreateTextureInfo upload_info;
		upload_info.name = "image before transfer to swapchain";
		upload_info.width = s_render_inst->render_io.screen_width;
		upload_info.height = s_render_inst->render_io.screen_height;
		upload_info.array_layers = 1;
		upload_info.format = RENDER_TARGET_IMAGE_FORMAT;
		upload_info.usage = IMAGE_USAGE::SWAPCHAIN_COPY_IMG;

		s_render_inst->frames = ArenaAllocArr(a_arena, RenderInterface_inst::Frame, s_render_inst->render_io.frame_count);
		for (size_t i = 0; i < s_render_inst->render_io.frame_count; i++)
		{
			s_render_inst->frames[i].render_target = CreateTexture(upload_info);
			s_render_inst->frames[i].graphics_queue_fence_value = 0;
		}
	}
	
	IMGUI_IMPL::ImInit(a_arena);

	s_render_inst->standard_3d_material = MaterialHandle(BB_INVALID_HANDLE_64);
	const uint64_t fence_value_transfer = s_render_inst->asset_uploader.next_fence_value.fetch_add(1);

	// setup cube positions
	{
		s_render_inst->cubemap_position = AllocateFromVertexBuffer(sizeof(SKYBOX_VERTICES));
		UploadBuffer cube_buffer = s_render_inst->asset_uploader.gpu_allocator.AllocateUploadMemory(sizeof(SKYBOX_VERTICES), fence_value_transfer);
		cube_buffer.SafeMemcpy(0, SKYBOX_VERTICES, sizeof(SKYBOX_VERTICES));
		RenderCopyBuffer copy_op;
		copy_op.src = cube_buffer.buffer;
		copy_op.dst = s_render_inst->cubemap_position.buffer;
		RenderCopyBufferRegion region_copy_op;
		region_copy_op.size = s_render_inst->cubemap_position.size;
		region_copy_op.src_offset = cube_buffer.base_offset;
		region_copy_op.dst_offset = s_render_inst->cubemap_position.offset;
		copy_op.regions = Slice(&region_copy_op, 1);
		Vulkan::CopyBuffer(list, copy_op);

		s_render_inst->global_buffer.data.cube_vertexpos_vertex_buffer_pos = static_cast<uint32_t>(s_render_inst->cubemap_position.offset);
	}

	start_up_pool.EndCommandList(list);
	//special upload here, due to uploading as well
	s_render_inst->graphics_queue.ReturnPool(start_up_pool);

	uint64_t fence_value_startup;
	s_render_inst->graphics_queue.ExecuteCommands(&list, 1, &s_render_inst->asset_uploader.fence, &fence_value_transfer, 1, nullptr, nullptr, 0, fence_value_startup);
	s_render_inst->graphics_queue.WaitFenceValue(fence_value_startup);
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

		CreateTextureInfo upload_info;
		upload_info.name = "image before transfer to swapchain";
		upload_info.width = s_render_inst->render_io.screen_width;
		upload_info.height = s_render_inst->render_io.screen_height;
		upload_info.array_layers = 1;
		upload_info.format = RENDER_TARGET_IMAGE_FORMAT;
		upload_info.usage = IMAGE_USAGE::SWAPCHAIN_COPY_IMG;

		s_render_inst->frames[i].render_target = CreateTexture(upload_info);
		s_render_inst->frames[i].graphics_queue_fence_value = 0;
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

void BB::StartFrame(const RCommandList a_list, const StartFrameInfo& a_info)
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
}

static bool uploading_assets = false;

static void UploadAssets(MemoryArena& a_thread_arena, void*)
{
	uploading_assets = true;

	CommandPool& cmd_pool = GetTransferCommandPool();
	const RCommandList list = cmd_pool.StartCommandList("upload asset list");

	if (!s_render_inst->asset_uploader.upload_meshes.IsEmpty())
	{
		MemoryArenaScope(a_thread_arena)
		{
			MeshHandle* mesh_handles = ArenaAllocArr(a_thread_arena, MeshHandle, MAX_MESH_UPLOAD_QUEUE);
			RenderCopyBufferRegion* vertex_regions = ArenaAllocArr(a_thread_arena, RenderCopyBufferRegion, MAX_MESH_UPLOAD_QUEUE);
			RenderCopyBufferRegion* index_regions = ArenaAllocArr(a_thread_arena, RenderCopyBufferRegion, MAX_MESH_UPLOAD_QUEUE);

			size_t index = 0;
			UploadDataMesh upload_data;
			while (s_render_inst->asset_uploader.upload_meshes.DeQueue(upload_data) &&
				index < MAX_MESH_UPLOAD_QUEUE)
			{
				mesh_handles[index] = upload_data.mesh_index;
				vertex_regions[index] = upload_data.vertex_region;
				index_regions[index] = upload_data.index_region;
				++index;
			}

			{
				RenderCopyBuffer vertex_copy{};
				vertex_copy.src = s_render_inst->asset_uploader.gpu_allocator.GetBuffer();
				vertex_copy.dst = s_render_inst->vertex_buffer.buffer;
				vertex_copy.regions = Slice(vertex_regions, index);

				Vulkan::CopyBuffer(list, vertex_copy);
			}
			{
				RenderCopyBuffer index_copy{};
				index_copy.src = s_render_inst->asset_uploader.gpu_allocator.GetBuffer();
				index_copy.dst = s_render_inst->index_buffer.buffer;
				index_copy.regions = Slice(index_regions, index);

				Vulkan::CopyBuffer(list, index_copy);
			}
		}
	}

	if (!s_render_inst->asset_uploader.upload_textures.IsEmpty())
	{
		MemoryArenaScope(a_thread_arena)
		{
			uint32_t write_info_count = 0;
			RenderCopyBufferToImageInfo* write_infos = ArenaAllocArr(a_thread_arena, RenderCopyBufferToImageInfo, MAX_TEXTURE_UPLOAD_QUEUE);
			uint32_t read_info_count = 0;
			RenderCopyImageToBufferInfo* read_infos = ArenaAllocArr(a_thread_arena, RenderCopyImageToBufferInfo, MAX_TEXTURE_UPLOAD_QUEUE);
			uint32_t before_trans_count = 0;
			PipelineBarrierImageInfo* before_transition = ArenaAllocArr(a_thread_arena, PipelineBarrierImageInfo, MAX_TEXTURE_UPLOAD_QUEUE);

			size_t index = 0;
			uint16_t base_layer = 0;
			uint16_t layer_count = 0;
			UploadDataTexture upload_data{};
			while (s_render_inst->asset_uploader.upload_textures.DeQueue(upload_data) &&
				index++ < MAX_TEXTURE_UPLOAD_QUEUE)
			{
				GPUTextureManager::TextureSlot& texture_slot = s_render_inst->texture_manager.GetTextureSlot(upload_data.texture_index);
				switch (upload_data.upload_type)
				{
				case UPLOAD_TEXTURE_TYPE::WRITE:
					write_infos[write_info_count++] = upload_data.write_info;
					base_layer = upload_data.write_info.dst_image_info.base_array_layer;
					layer_count = upload_data.write_info.dst_image_info.layer_count;
					break;
				case UPLOAD_TEXTURE_TYPE::READ:
					read_infos[read_info_count++] = upload_data.read_info;
					base_layer = upload_data.read_info.src_image_info.base_array_layer;
					layer_count = upload_data.read_info.src_image_info.layer_count;
					break;
				default:
					BB_ASSERT(false, "invalid upload UPLOAD_TEXTURE_TYPE value or unimplemented switch statement");
					break;
				}

				if (upload_data.start_layout != IMAGE_LAYOUT::TRANSFER_DST)
				{
					PipelineBarrierImageInfo& pi = before_transition[before_trans_count++];
					pi.src_mask = BARRIER_ACCESS_MASK::NONE;
					pi.dst_mask = BARRIER_ACCESS_MASK::TRANSFER_WRITE;
					pi.image = texture_slot.texture_info.image;
					pi.old_layout = upload_data.start_layout;
					pi.new_layout = IMAGE_LAYOUT::TRANSFER_DST;
					pi.src_queue = QUEUE_TRANSITION::NO_TRANSITION;
					pi.dst_queue = QUEUE_TRANSITION::NO_TRANSITION;
					pi.layer_count = layer_count;
					pi.level_count = 1;
					pi.base_array_layer = base_layer;
					pi.base_mip_level = 0;
					pi.src_stage = BARRIER_PIPELINE_STAGE::TOP_OF_PIPELINE;
					pi.dst_stage = BARRIER_PIPELINE_STAGE::TRANSFER;
				}

				if (upload_data.end_layout == IMAGE_LAYOUT::TRANSFER_DST)
				{
					PipelineBarrierImageInfo pi{};
					pi.src_mask = BARRIER_ACCESS_MASK::TRANSFER_WRITE;
					pi.dst_mask = BARRIER_ACCESS_MASK::SHADER_READ;
					pi.image = texture_slot.texture_info.image;
					pi.old_layout = IMAGE_LAYOUT::TRANSFER_DST;
					pi.new_layout = upload_data.end_layout;
					pi.src_queue = QUEUE_TRANSITION::NO_TRANSITION;
					pi.dst_queue = QUEUE_TRANSITION::NO_TRANSITION;
					pi.layer_count = layer_count;
					pi.level_count = 1;
					pi.base_array_layer = base_layer;
					pi.base_mip_level = 0;
					pi.src_stage = BARRIER_PIPELINE_STAGE::TRANSFER;
					pi.dst_stage = BARRIER_PIPELINE_STAGE::FRAGMENT_SHADER;

					WriteDescriptorData write_data{};
					write_data.binding = GLOBAL_BINDLESS_TEXTURES_BINDING;
					write_data.descriptor_index = upload_data.texture_index.handle;
					write_data.type = DESCRIPTOR_TYPE::IMAGE;
					write_data.image_view.view = texture_slot.texture_info.view;
					write_data.image_view.layout = IMAGE_LAYOUT::SHADER_READ_ONLY;

					s_render_inst->texture_manager.AddGraphicsTransition(pi, write_data);
				}
			}

			PipelineBarrierInfo pipeline_info{};
			pipeline_info.image_info_count = before_trans_count;
			pipeline_info.image_infos = before_transition;
			Vulkan::PipelineBarriers(list, pipeline_info);

			for (size_t i = 0; i < write_info_count; i++)
			{
				Vulkan::CopyBufferToImage(list, write_infos[i]);
			}

			for (size_t i = 0; i < read_info_count; i++)
			{
				Vulkan::CopyImageToBuffer(list, read_infos[i]);
			}
		}
	}

	cmd_pool.EndCommandList(list);

	s_render_inst->transfer_queue.ReturnPools(Slice(&cmd_pool, 1));
	uint64_t mock_fence;	// TODO, remove this
	const uint64_t asset_fence_value = s_render_inst->asset_uploader.next_fence_value.fetch_add(1);
	s_render_inst->transfer_queue.ExecuteCommands(&list, 1, &s_render_inst->asset_uploader.fence, &asset_fence_value, 1, nullptr, nullptr, 0, mock_fence);

	uploading_assets = false;
}

void BB::EndFrame(const RCommandList a_list, const ShaderEffectHandle a_imgui_vertex, const ShaderEffectHandle a_imgui_fragment, bool a_skip)
{
	BB_ASSERT(s_render_inst->render_io.frame_started == true, "did not call StartFrame before a EndFrame");

	if ((!s_render_inst->asset_uploader.upload_meshes.IsEmpty() || !s_render_inst->asset_uploader.upload_textures.IsEmpty()) &&
		!uploading_assets)
	{
		Threads::StartTaskThread(UploadAssets, nullptr, L"upload assets task");
	}

	if (a_skip || s_render_inst->render_io.resizing_request)
	{
		ImGui::EndFrame();
		return;
	}
	const uint32_t frame_index = s_render_inst->render_io.frame_index;
	const RenderInterface_inst::Frame& cur_frame = s_render_inst->frames[frame_index];
	const GPUTextureManager::TextureSlot render_target = s_render_inst->texture_manager.GetTextureSlot(cur_frame.render_target);

	ImGui::Render();
	IMGUI_IMPL::ImRenderFrame(a_list, render_target.texture_info.view, true, a_imgui_vertex, a_imgui_fragment);
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

	s_render_inst->texture_manager.TransitionTextures(a_list);

	const int2 swapchain_size(static_cast<int>(s_render_inst->render_io.screen_width), static_cast<int>(s_render_inst->render_io.screen_height));

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

	CreateTextureInfo texture_info;
	texture_info.width = a_render_target_extent.x;
	texture_info.height = a_render_target_extent.y;
	texture_info.name = a_name;
	texture_info.format = RENDER_TARGET_IMAGE_FORMAT;
	texture_info.usage = IMAGE_USAGE::RENDER_TARGET;
	texture_info.array_layers = 1;

	for (uint32_t i = 0; i < s_render_inst->render_io.frame_count; i++)
	{
		viewport->render_targets[i] = CreateTexture(texture_info);
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

	CreateTextureInfo texture_info;
	texture_info.width = a_render_target_extent.x;
	texture_info.height = a_render_target_extent.y;
	texture_info.name = viewport->name;
	texture_info.format = RENDER_TARGET_IMAGE_FORMAT;
	texture_info.usage = IMAGE_USAGE::RENDER_TARGET;
	texture_info.array_layers = 1;

	for (uint32_t i = 0; i < s_render_inst->render_io.frame_count; i++)
	{
		FreeTexture(viewport->render_targets[i]);
		viewport->render_targets[i] = CreateTexture(texture_info);
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

void BB::StartRenderPass(const RCommandList a_list, const StartRenderingInfo& a_render_info)
{
	Vulkan::StartRenderPass(a_list, a_render_info);
}

void BB::EndRenderPass(const RCommandList a_list)
{
	Vulkan::EndRenderPass(a_list);
}

RPipelineLayout BB::BindShaders(MemoryArena& a_temp_arena, const RCommandList a_list, const Slice<ShaderEffectHandle> a_shader_effects)
{
	ShaderObject* const shader_objects = ArenaAllocArr(a_temp_arena, ShaderObject, a_shader_effects.size());
	SHADER_STAGE* const shader_stages = ArenaAllocArr(a_temp_arena, SHADER_STAGE, a_shader_effects.size());

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

void BB::SetFrontFace(const RCommandList a_list, const bool a_is_clockwise)
{
	Vulkan::SetFrontFace(a_list, a_is_clockwise);
}

void BB::SetCullMode(const RCommandList a_list, const CULL_MODE a_cull_mode)
{
	Vulkan::SetCullMode(a_list, a_cull_mode);
}

void BB::SetClearColor(const RCommandList a_list, const float3 a_clear_color)
{
	Vulkan::SetClearColor(a_list, a_clear_color);
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

RenderScene3DHandle BB::Create3DRenderScene(MemoryArena& a_arena, const SceneCreateInfo& a_info, const char* a_name)
{
	constexpr uint32_t scene_size = sizeof(Scene3DInfo);
	const uint32_t shader_transform_size = scene_3d->draw_list_max * sizeof(ShaderTransform);
	const uint32_t light_buffer_size = scene_3d->light_container.capacity() * sizeof(Light);

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

void BB::RenderScenePerDraw(const RCommandList a_cmd_list, const RenderScene3DHandle a_scene, const RenderTarget a_render_target, const uint2 a_draw_area_size, const int2 a_draw_area_offset, const Slice<const ShaderEffectHandle> a_shader_effects)
{
	const RTexture render_target_texture = reinterpret_cast<RenderTargetStruct*>(a_render_target.handle)->GetTargetTexture();
	const GPUTextureManager::TextureSlot render_target = s_render_inst->texture_manager.GetTextureSlot(render_target_texture);

	const Scene3D& render_scene3d = *reinterpret_cast<Scene3D*>(a_scene.handle);
	const auto& scene_frame = render_scene3d.frames[s_render_inst->render_io.frame_index];

}

void BB::EndRenderScene(const RCommandList a_cmd_list, const RenderScene3DHandle a_scene, const RenderTarget a_render_target, const uint2 a_draw_area_size, const int2 a_draw_area_offset, bool a_skip)
{
	Scene3D& render_scene3d = *reinterpret_cast<Scene3D*>(a_scene.handle);
	const auto& scene_frame = render_scene3d.frames[s_render_inst->render_io.frame_index];

	if (render_scene3d.draw_list_count == 0 || a_skip)
	{
		return;
	}



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

	RenderingAttachmentColor color_attach{};
	color_attach.load_color = true;
	color_attach.store_color = true;
	color_attach.image_layout = IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL;
	color_attach.image_view = render_target.texture_info.view;

	RenderingAttachmentDepth depth_attach{};
	depth_attach.load_depth = false;
	depth_attach.store_depth = true;
	depth_attach.image_layout = IMAGE_LAYOUT::DEPTH_STENCIL_ATTACHMENT;
	depth_attach.image_view = render_scene3d.depth_image_view;

	StartRenderingInfo start_rendering_info{};
	start_rendering_info.render_area_extent = a_draw_area_size;
	start_rendering_info.render_area_offset = a_draw_area_offset;
	start_rendering_info.color_attachments = Slice(&color_attach, 1);
	start_rendering_info.depth_attachment = &depth_attach;
	Vulkan::StartRenderPass(a_cmd_list, start_rendering_info);

	{
		//set the first data to get the first 3 descriptor sets.
		const MeshDrawCall& mesh_draw_call = render_scene3d.draw_list_data.mesh_draw_call[0];
		const Material& material = s_render_inst->material_map[mesh_draw_call.material];

		//set 0, do this at start
		Vulkan::SetDescriptorImmutableSamplers(a_cmd_list, material.shader.pipeline_layout);
	}

	Vulkan::BindIndexBuffer(a_cmd_list, s_render_inst->index_buffer.buffer, 0);
	Vulkan::SetFrontFace(a_cmd_list, false);
	Vulkan::SetCullMode(a_cmd_list, CULL_MODE::NONE);
	for (uint32_t i = 0; i < render_scene3d.draw_list_count; i++)
	{
		const MeshDrawCall& mesh_draw_call = render_scene3d.draw_list_data.mesh_draw_call[i];
		const Material& material = s_render_inst->material_map[mesh_draw_call.material];
		const Mesh& mesh = s_render_inst->mesh_map.find(mesh_draw_call.mesh);

		ShaderObject shader_objects[2];
		SHADER_STAGE shader_stages[2];

		for (size_t eff_index = 0; eff_index < 2; eff_index++)
		{
			const ShaderEffect& effect = s_render_inst->shader_effects[material.shader.shader_effects[eff_index]];
			shader_objects[eff_index] = effect.shader_object;
			shader_stages[eff_index] = effect.shader_stage;
		}

		Vulkan::BindShaders(a_cmd_list,
			material.shader.shader_effect_count,
			shader_stages,
			shader_objects);

		ShaderIndices shader_indices;
		shader_indices.transform_index = i;
		shader_indices.vertex_buffer_offset = static_cast<uint32_t>(mesh.vertex_buffer.offset);
		shader_indices.albedo_texture = mesh_draw_call.base_texture.handle;
		shader_indices.normal_texture = mesh_draw_call.normal_texture.handle;

		Vulkan::SetPushConstants(a_cmd_list, material.shader.pipeline_layout, 0, sizeof(ShaderIndices), &shader_indices);
		Vulkan::DrawIndexed(a_cmd_list,
			mesh_draw_call.index_count,
			1,
			static_cast<uint32_t>(mesh.index_buffer.offset / sizeof(uint32_t)) + mesh_draw_call.index_start,
			0,
			0);
	}

	Vulkan::EndRenderPass(a_cmd_list);
}

CommandPool& BB::GetGraphicsCommandPool()
{
	return s_render_inst->graphics_queue.GetCommandPool();
}

CommandPool& BB::GetTransferCommandPool()
{
	return s_render_inst->transfer_queue.GetCommandPool();
}

bool BB::PresentFrame(const BB::Slice<CommandPool> a_cmd_pools, uint64_t& a_fence_value)
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
	const PRESENT_IMAGE_RESULT result = s_render_inst->graphics_queue.ExecutePresentCommands(lists, list_count, nullptr, nullptr, 0, nullptr, nullptr, 0, s_render_inst->render_io.frame_index, a_fence_value);
	s_render_inst->render_io.frame_index = (s_render_inst->render_io.frame_index + 1) % s_render_inst->render_io.frame_count;

	s_render_inst->render_io.frame_ended = false;
	s_render_inst->render_io.frame_started = false;

	if (result == PRESENT_IMAGE_RESULT::SWAPCHAIN_OUT_OF_DATE)
		s_render_inst->render_io.resizing_request = true;

	return true;
}

bool BB::ExecuteGraphicCommands(const BB::Slice<CommandPool> a_cmd_pools, uint64_t& a_out_fence_value)
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
	s_render_inst->graphics_queue.ExecuteCommands(lists, list_count, nullptr, nullptr, 0, nullptr, nullptr, 0, a_out_fence_value);
	return true;
}

const MeshHandle BB::CreateMesh(const CreateMeshInfo& a_create_info)
{
	// make this it's own class or something
	RenderInterface_inst::AssetUploader& uploader = s_render_inst->asset_uploader;

	UploadBuffer upload_buffer = uploader.gpu_allocator.AllocateUploadMemory(a_create_info.vertices.sizeInBytes() + a_create_info.indices.sizeInBytes(), uploader.next_fence_value);

	Mesh mesh;
	mesh.vertex_buffer = AllocateFromVertexBuffer(a_create_info.vertices.sizeInBytes());
	mesh.index_buffer = AllocateFromIndexBuffer(a_create_info.indices.sizeInBytes());

	const size_t vertex_offset = 0;
	const size_t index_offset = a_create_info.vertices.sizeInBytes();
	upload_buffer.SafeMemcpy(vertex_offset, a_create_info.vertices.data(), a_create_info.vertices.sizeInBytes());
	upload_buffer.SafeMemcpy(index_offset, a_create_info.indices.data(), a_create_info.indices.sizeInBytes());

	UploadDataMesh task{};
	task.vertex_region.size = mesh.vertex_buffer.size;
	task.vertex_region.dst_offset = mesh.vertex_buffer.offset;
	task.vertex_region.src_offset = vertex_offset + upload_buffer.base_offset;
	task.index_region.size = mesh.index_buffer.size;
	task.index_region.dst_offset = mesh.index_buffer.offset;
	task.index_region.src_offset = index_offset + upload_buffer.base_offset;
	task.mesh_index = s_render_inst->mesh_map.insert(mesh); // TODO, place a debug mesh in mesh_index!
	uploader.upload_meshes.EnQueue(task);
	return task.mesh_index;
}

void BB::FreeMesh(const MeshHandle a_mesh)
{
	s_render_inst->mesh_map.erase(a_mesh);
}

RDescriptorLayout BB::CreateDescriptorLayout(MemoryArena& a_temp_arena, Slice<DescriptorBindingInfo> a_bindings)
{
	return Vulkan::CreateDescriptorLayout(a_temp_arena, a_bindings);
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
			a_create_infos[i].shader_data,
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
		shader_effects[i].name = a_create_infos[i].name;
		shader_effects[i].shader_object = shader_objects[i];
		shader_effects[i].shader_stage = a_create_infos[i].stage;
		shader_effects[i].shader_stages_next = a_create_infos[i].next_stages;
#ifdef _ENABLE_REBUILD_SHADERS
		shader_effects[i].create_info = shader_object_infos[i];
		shader_effects[i].shader_entry = a_create_infos[i].shader_entry;
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
	Vulkan::DestroyShaderObject(old_effect.shader_object);

	const ShaderCode shader_code = CompileShader(s_render_inst->shader_compiler,
		a_shader,
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
	const RPipelineLayout chosen_layout = s_render_inst->shader_effects[a_create_info.shader_effects[0]].pipeline_layout;
	
	SHADER_STAGE_FLAGS valid_next_stages = static_cast<uint32_t>(SHADER_STAGE::ALL);
	for (size_t i = 0; i < a_create_info.shader_effects.size(); i++)
	{
		//maybe check if we have duplicate shader stages;
		const ShaderEffect& effect = s_render_inst->shader_effects[a_create_info.shader_effects[i]];
		BB_ASSERT(chosen_layout == effect.pipeline_layout, "pipeline layouts are not the same for the shader effects");
		
		if (i < a_create_info.shader_effects.size())
		{
			BB_ASSERT((valid_next_stages & static_cast<SHADER_STAGE_FLAGS>(effect.shader_stage)) == static_cast<SHADER_STAGE_FLAGS>(effect.shader_stage), 
				"shader stage is not valid for the next shader stage of the previous shader object");
			valid_next_stages = effect.shader_stages_next;
		}

		mat.shader.shader_effects[i] = a_create_info.shader_effects[i];
	}
	mat.name = a_create_info.name;
	mat.shader.shader_effect_count = static_cast<uint32_t>(a_create_info.shader_effects.size());
	mat.shader.pipeline_layout = chosen_layout;

	
	return s_render_inst->material_map.emplace(mat);
}

void BB::FreeMaterial(const MaterialHandle a_material)
{
	// maybe go and check the refcount of the textures to possibly free them.
	s_render_inst->material_map.erase(a_material);
}

const RTexture BB::CreateTexture(const CreateTextureInfo& a_create_info)
{
	TextureInfo tex_info;
	tex_info.width = a_create_info.width;
	tex_info.height = a_create_info.height;
	tex_info.array_layers = a_create_info.array_layers;
	tex_info.mip_levels = 1;
	tex_info.format = a_create_info.format;
	tex_info.current_layout = IMAGE_LAYOUT::UNDEFINED;

	// not required to be remembered
	tex_info.usage = a_create_info.usage;

	{
		ImageCreateInfo image_info;
		image_info.name = a_create_info.name;
		image_info.width = a_create_info.width;
		image_info.height = a_create_info.height;
		image_info.depth = 1;
		image_info.array_layers = a_create_info.array_layers;
		image_info.mip_levels = 1;
		image_info.type = IMAGE_TYPE::TYPE_2D;
		image_info.tiling = IMAGE_TILING::OPTIMAL;
		image_info.format = a_create_info.format;
		image_info.usage = a_create_info.usage;
		tex_info.image = Vulkan::CreateImage(image_info);

		ImageViewCreateInfo image_view_info;
		image_view_info.image = tex_info.image;
		image_view_info.name = a_create_info.name;
		image_view_info.array_layers = a_create_info.array_layers;
		image_view_info.mip_levels = 1;
		image_view_info.type = IMAGE_VIEW_TYPE::TYPE_2D;
		image_view_info.format = image_info.format;
		tex_info.view = Vulkan::CreateViewImage(image_view_info);
	}

	return s_render_inst->texture_manager.SetTextureSlot(tex_info, a_create_info.name);
}

const RTexture BB::CreateTextureCubeMap(const CreateTextureInfo& a_create_info)
{
	BB_ASSERT(a_create_info.array_layers % 6 == 0, "creating a cubemap but the array_layers are not a multiple of 6");
	TextureInfo tex_info;
	tex_info.width = a_create_info.width;
	tex_info.height = a_create_info.height;
	tex_info.array_layers = a_create_info.array_layers;
	tex_info.mip_levels = 1;
	tex_info.format = a_create_info.format;
	tex_info.current_layout = IMAGE_LAYOUT::UNDEFINED;

	// not required to be remembered
	tex_info.usage = a_create_info.usage;

	{
		ImageCreateInfo image_info;
		image_info.name = a_create_info.name;
		image_info.width = a_create_info.width;
		image_info.height = a_create_info.height;
		image_info.depth = 1;
		image_info.array_layers = a_create_info.array_layers;
		image_info.mip_levels = 1;
		image_info.type = IMAGE_TYPE::TYPE_2D;
		image_info.tiling = IMAGE_TILING::OPTIMAL;
		image_info.format = a_create_info.format;
		image_info.usage = a_create_info.usage;
		image_info.is_cube_map = true;
		tex_info.image = Vulkan::CreateImage(image_info);

		ImageViewCreateInfo image_view_info;
		image_view_info.image = tex_info.image;
		image_view_info.name = a_create_info.name;
		image_view_info.array_layers = a_create_info.array_layers;
		image_view_info.mip_levels = 1;
		image_view_info.type = IMAGE_VIEW_TYPE::CUBE;
		image_view_info.format = image_info.format;
		tex_info.view = Vulkan::CreateViewImage(image_view_info);
	}

	return s_render_inst->texture_manager.SetTextureSlot(tex_info, a_create_info.name);
}

static inline bool IsImageWithinBounds(const GPUTextureManager::TextureSlot& a_slot, const uint2 a_extent, const int2 a_offset)
{
	if (a_slot.texture_info.width >= a_extent.x + static_cast<uint32_t>(a_offset.x) &&
		a_slot.texture_info.height >= a_extent.y + static_cast<uint32_t>(a_offset.y))
		return true;
	return false;
}

void BB::BlitTexture(const RCommandList a_list, const BlitTextureInfo& a_blit_info)
{
	// use with new asset system, but this is not used now.
	BB_UNIMPLEMENTED();

	GPUTextureManager::TextureSlot& src_texture = s_render_inst->texture_manager.GetTextureSlot(a_blit_info.src);
	GPUTextureManager::TextureSlot& dst_texture = s_render_inst->texture_manager.GetTextureSlot(a_blit_info.dst);

	BlitImageInfo blit_image;
	blit_image.src_image = src_texture.texture_info.image;
	blit_image.src_offset_p0 = int3(a_blit_info.src_point_0.x, a_blit_info.src_point_0.y, 0);
	blit_image.src_offset_p1 = int3(a_blit_info.src_point_1.x, a_blit_info.src_point_1.y, 1);
	blit_image.src_base_layer = 0;
	blit_image.src_mip_level = 0;
	blit_image.src_layer_count = 1;

	blit_image.dst_image = dst_texture.texture_info.image;
	blit_image.dst_offset_p0 = int3(a_blit_info.dst_point_0.x, a_blit_info.dst_point_0.y, 0);
	blit_image.dst_offset_p1 = int3(a_blit_info.dst_point_1.x, a_blit_info.dst_point_1.y, 1);
	blit_image.dst_base_layer = 0;
	blit_image.dst_mip_level = 0;
	blit_image.dst_layer_count = 1;

	Vulkan::BlitImage(a_list, blit_image);
}

void BB::CopyTexture(const RCommandList a_list, const CopyTextureInfo& a_copy_info)
{
	// use with new asset system, but this is not used.
	BB_UNIMPLEMENTED();
	GPUTextureManager::TextureSlot& src_texture = s_render_inst->texture_manager.GetTextureSlot(a_copy_info.src);
	GPUTextureManager::TextureSlot& dst_texture = s_render_inst->texture_manager.GetTextureSlot(a_copy_info.dst);

	BB_ASSERT(IsImageWithinBounds(src_texture, uint2(a_copy_info.extent.x, a_copy_info.extent.y), int2(a_copy_info.src_copy_info.offset_x, a_copy_info.src_copy_info.offset_y)), "src image out of bounds");
	BB_ASSERT(IsImageWithinBounds(dst_texture, uint2(a_copy_info.extent.x, a_copy_info.extent.y), int2(a_copy_info.dst_copy_info.offset_x, a_copy_info.dst_copy_info.offset_y)), "dst image out of bounds");

	RenderCopyImage render_copy_image;
	render_copy_image.extent = a_copy_info.extent;
	render_copy_image.src_image = src_texture.texture_info.image;
	render_copy_image.src_copy_info = a_copy_info.src_copy_info;
	render_copy_image.dst_image = dst_texture.texture_info.image;
	render_copy_image.dst_copy_info = a_copy_info.dst_copy_info;
	Vulkan::CopyImage(a_list, render_copy_image);
}

GPUFenceValue BB::WriteTexture(const RTexture a_texture, const WriteTextureInfo& a_write_info)
{
	const GPUTextureManager::TextureSlot& texture_slot = s_render_inst->texture_manager.GetTextureSlot(a_texture);
	BB_ASSERT(IsImageWithinBounds(texture_slot, a_write_info.extent, a_write_info.offset), "write image out of bounds");

	RenderInterface_inst::AssetUploader& uploader = s_render_inst->asset_uploader;

	const size_t byte_per_pixel = GetByteSizeOfImageFormat(texture_slot.texture_info.format);
	//now upload the image.
	UploadBuffer upload_buffer = uploader.gpu_allocator.AllocateUploadMemory(byte_per_pixel * a_write_info.extent.x * a_write_info.extent.y, uploader.next_fence_value);
	upload_buffer.SafeMemcpy(0, a_write_info.pixels, byte_per_pixel * a_write_info.extent.x * a_write_info.extent.y);

	RenderCopyBufferToImageInfo buffer_to_image;
	buffer_to_image.src_buffer = upload_buffer.buffer;
	buffer_to_image.src_offset = static_cast<uint32_t>(upload_buffer.base_offset);

	buffer_to_image.dst_image = texture_slot.texture_info.image;
	buffer_to_image.dst_extent.x = a_write_info.extent.x;
	buffer_to_image.dst_extent.y = a_write_info.extent.y;
	buffer_to_image.dst_extent.z = 1;
	buffer_to_image.dst_image_info.offset_x = a_write_info.offset.x;
	buffer_to_image.dst_image_info.offset_y = a_write_info.offset.y;
	buffer_to_image.dst_image_info.offset_z = 0;
	buffer_to_image.dst_image_info.mip_level = 0;
	buffer_to_image.dst_image_info.layer_count = a_write_info.layer_count;
	buffer_to_image.dst_image_info.base_array_layer = a_write_info.base_array_layer;

	UploadDataTexture upload_texture{};
	upload_texture.texture_index = a_texture;
	upload_texture.upload_type = UPLOAD_TEXTURE_TYPE::WRITE;
	upload_texture.write_info = buffer_to_image;
	upload_texture.start_layout = IMAGE_LAYOUT::UNDEFINED;
	upload_texture.end_layout = a_write_info.set_shader_visible ? IMAGE_LAYOUT::SHADER_READ_ONLY : IMAGE_LAYOUT::TRANSFER_DST;
	uploader.upload_textures.EnQueue(upload_texture);

	return GPUFenceValue(uploader.next_fence_value.load());
}

void BB::FreeTexture(const RTexture a_texture)
{
	return s_render_inst->texture_manager.FreeTexture(a_texture);
}

GPUFenceValue BB::ReadTexture(const RTexture a_texture, const uint2 a_extent, const int2 a_offset, const GPUBuffer a_readback_buffer, const size_t a_readback_buffer_size)
{
	const GPUTextureManager::TextureSlot& selected_texture = s_render_inst->texture_manager.GetTextureSlot(a_texture);
	BB_ASSERT(selected_texture.texture_info.format == SCREENSHOT_IMAGE_FORMAT, "image format is not SRGB, image write may not be successful");
	BB_ASSERT(IsImageWithinBounds(selected_texture, a_extent, a_offset), "reading texture out of bounds!");

	const IMAGE_LAYOUT original_layout = selected_texture.texture_info.current_layout;

	RenderInterface_inst::AssetUploader& uploader = s_render_inst->asset_uploader;

	const size_t image_size =
		static_cast<size_t>(a_extent.x) *
		static_cast<size_t>(a_extent.y) *
		SCREENSHOT_IMAGE_PIXEL_BYTE_SIZE;

	BB_ASSERT(image_size <= a_readback_buffer_size, "readback buffer too small");

	RenderCopyImageToBufferInfo image_to_buffer;
	image_to_buffer.dst_buffer = a_readback_buffer;
	image_to_buffer.dst_offset = 0; // change this if i have a global readback buffer thing

	image_to_buffer.src_image = selected_texture.texture_info.image;
	image_to_buffer.src_extent.x = a_extent.x;
	image_to_buffer.src_extent.y = a_extent.y;
	image_to_buffer.src_extent.z = 1;
	image_to_buffer.src_image_info.offset_x = a_offset.x;
	image_to_buffer.src_image_info.offset_y = a_offset.y;
	image_to_buffer.src_image_info.offset_z = 0;
	image_to_buffer.src_image_info.mip_level = 0;
	image_to_buffer.src_image_info.layer_count = 1;
	image_to_buffer.src_image_info.base_array_layer = 0;

	UploadDataTexture upload_texture{};
	upload_texture.texture_index = a_texture;
	upload_texture.upload_type = UPLOAD_TEXTURE_TYPE::READ;
	upload_texture.read_info = image_to_buffer;
	upload_texture.start_layout = original_layout;
	upload_texture.end_layout = original_layout;
	uploader.upload_textures.EnQueue(upload_texture);

	return GPUFenceValue(uploader.next_fence_value.load());
}

GPUFenceValue BB::GetTransferFenceValue()
{
	return GPUFenceValue(s_render_inst->asset_uploader.next_fence_value.load() - 1);
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

void BB::DescriptorWriteImageBuffer(const DescriptorWriteImageInfo& a_write_info)
{
	Vulkan::DescriptorWriteImageBuffer(a_write_info);
}

RFence CreateFence(const uint64_t a_initial_value, const char* a_name)
{
	return Vulkan::CreateFence(a_initial_value, a_name);
}

void FreeFence(const RFence a_fence)
{
	Vulkan::FreeFence(a_fence);
}

void WaitFence(const RFence a_fence, const uint64_t a_fence_value)
{
	Vulkan::WaitFence(a_fence, a_fence_value);
}

void WaitFences(const RFence* a_fences, const uint64_t* a_fence_values, const uint32_t a_fence_count)
{
	Vulkan::WaitFences(a_fences, a_fence_values, a_fence_count);
}

uint64_t GetCurrentFenceValue(const RFence a_fence)
{
	return Vulkan::GetCurrentFenceValue(a_fence);
}

void BB::SetDescriptorBufferOffset(const RCommandList a_list, const RPipelineLayout a_pipe_layout, const uint32_t a_first_set, const uint32_t a_set_count, const uint32_t* a_buffer_indices, const size_t* a_offsets)
{
	Vulkan::SetDescriptorBufferOffset(a_list, a_pipe_layout, a_first_set, a_set_count, a_buffer_indices, a_offsets);
}

const BB::DescriptorAllocation& GetGlobalDescriptorAllocation()
{
	return s_render_inst->global_descriptor_allocation;
}

bool BB::SetDefaultMaterial(const MaterialHandle a_material)
{
	if (!a_material.IsValid())
		return false;

	s_render_inst->standard_3d_material = a_material;
	return true;
}

MaterialHandle BB::GetStandardMaterial()
{
	return s_render_inst->standard_3d_material;
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
