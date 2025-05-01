#include "ImGuiImpl.hpp"
#include "Renderer.hpp"
#include "AssetLoader.hpp"

#include "imgui.h"
#include "implot.h"

#include "HID.h"

using namespace BB;

constexpr size_t INITIAL_VERTEX_SIZE = sizeof(Vertex2D) * 128;
constexpr size_t INITIAL_INDEX_SIZE = sizeof(uint32_t) * 256;

struct ImInputData
{
	BB::WindowHandle            window;
	int                         MouseTrackedArea;   // 0: not tracked, 1: client are, 2: non-client area
	int                         MouseButtonsDown;
	int64_t                     Time;
	int64_t                     TicksPerSecond;
	ImGuiMouseCursor            LastMouseCursor;
};

static ImInputData* ImGetInputData()
{
	return ImGui::GetCurrentContext() ? reinterpret_cast<ImInputData*>(ImGui::GetIO().BackendPlatformUserData) : nullptr;
}

struct ImRenderBuffer
{
	WriteableGPUBufferView vertex_buffer;
	WriteableGPUBufferView index_buffer;
};

struct ImRenderData
{
	AssetHandle font;				 // 8
	RDescriptorIndex font_descriptor;// 12

	// Render buffers for main window
	uint32_t frame_index;            // 16
	ImRenderBuffer* frame_buffers;	 // 24
};

inline static ImRenderData* ImGetRenderData()
{
	return ImGui::GetCurrentContext() ? reinterpret_cast<ImRenderData*>(ImGui::GetIO().BackendRendererUserData) : nullptr;
}

//BB FRAMEWORK TEMPLATE, MAY CHANGE THIS.
static ImGuiKey ImBBKeyToImGuiKey(const KEYBOARD_KEY a_Key)
{
	switch (a_Key)
	{
	case KEYBOARD_KEY::TAB: return ImGuiKey_Tab;
	case KEYBOARD_KEY::BACKSPACE: return ImGuiKey_Backspace;
	case KEYBOARD_KEY::SPACEBAR: return ImGuiKey_Space;
	case KEYBOARD_KEY::RETURN: return ImGuiKey_Enter;
	case KEYBOARD_KEY::ESCAPE: return ImGuiKey_Escape;
	case KEYBOARD_KEY::APOSTROPHE: return ImGuiKey_Apostrophe;
	case KEYBOARD_KEY::COMMA: return ImGuiKey_Comma;
	case KEYBOARD_KEY::MINUS: return ImGuiKey_Minus;
	case KEYBOARD_KEY::PERIOD: return ImGuiKey_Period;
	case KEYBOARD_KEY::SLASH: return ImGuiKey_Slash;
	case KEYBOARD_KEY::SEMICOLON: return ImGuiKey_Semicolon;
	case KEYBOARD_KEY::EQUALS: return ImGuiKey_Equal;
	case KEYBOARD_KEY::BRACKETLEFT: return ImGuiKey_LeftBracket;
	case KEYBOARD_KEY::BACKSLASH: return ImGuiKey_Backslash;
	case KEYBOARD_KEY::BRACKETRIGHT: return ImGuiKey_RightBracket;
	case KEYBOARD_KEY::GRAVE: return ImGuiKey_GraveAccent;
	case KEYBOARD_KEY::CAPSLOCK: return ImGuiKey_CapsLock;
	case KEYBOARD_KEY::NUMPADMULTIPLY: return ImGuiKey_KeypadMultiply;
	case KEYBOARD_KEY::SHIFTLEFT: return ImGuiKey_LeftShift;
	case KEYBOARD_KEY::CONTROLLEFT: return ImGuiKey_LeftCtrl;
	case KEYBOARD_KEY::ALTLEFT: return ImGuiKey_LeftAlt;
	case KEYBOARD_KEY::SHIFTRIGHT: return ImGuiKey_RightShift;
	case KEYBOARD_KEY::KEY_0: return ImGuiKey_0;
	case KEYBOARD_KEY::KEY_1: return ImGuiKey_1;
	case KEYBOARD_KEY::KEY_2: return ImGuiKey_2;
	case KEYBOARD_KEY::KEY_3: return ImGuiKey_3;
	case KEYBOARD_KEY::KEY_4: return ImGuiKey_4;
	case KEYBOARD_KEY::KEY_5: return ImGuiKey_5;
	case KEYBOARD_KEY::KEY_6: return ImGuiKey_6;
	case KEYBOARD_KEY::KEY_7: return ImGuiKey_7;
	case KEYBOARD_KEY::KEY_8: return ImGuiKey_8;
	case KEYBOARD_KEY::KEY_9: return ImGuiKey_9;
	case KEYBOARD_KEY::A: return ImGuiKey_A;
	case KEYBOARD_KEY::B: return ImGuiKey_B;
	case KEYBOARD_KEY::C: return ImGuiKey_C;
	case KEYBOARD_KEY::D: return ImGuiKey_D;
	case KEYBOARD_KEY::E: return ImGuiKey_E;
	case KEYBOARD_KEY::F: return ImGuiKey_F;
	case KEYBOARD_KEY::G: return ImGuiKey_G;
	case KEYBOARD_KEY::H: return ImGuiKey_H;
	case KEYBOARD_KEY::I: return ImGuiKey_I;
	case KEYBOARD_KEY::J: return ImGuiKey_J;
	case KEYBOARD_KEY::K: return ImGuiKey_K;
	case KEYBOARD_KEY::L: return ImGuiKey_L;
	case KEYBOARD_KEY::M: return ImGuiKey_M;
	case KEYBOARD_KEY::N: return ImGuiKey_N;
	case KEYBOARD_KEY::O: return ImGuiKey_O;
	case KEYBOARD_KEY::P: return ImGuiKey_P;
	case KEYBOARD_KEY::Q: return ImGuiKey_Q;
	case KEYBOARD_KEY::R: return ImGuiKey_R;
	case KEYBOARD_KEY::S: return ImGuiKey_S;
	case KEYBOARD_KEY::T: return ImGuiKey_T;
	case KEYBOARD_KEY::U: return ImGuiKey_U;
	case KEYBOARD_KEY::V: return ImGuiKey_V;
	case KEYBOARD_KEY::W: return ImGuiKey_W;
	case KEYBOARD_KEY::X: return ImGuiKey_X;
	case KEYBOARD_KEY::Y: return ImGuiKey_Y;
	case KEYBOARD_KEY::Z: return ImGuiKey_Z;
	default: return ImGuiKey_None;
	}
}

static inline void SetupImGuiInput(MemoryArena& a_arena, const BB::WindowHandle a_window)
{
	ImGuiIO& io = ImGui::GetIO();

	// Setup backend capabilities flags
	ImInputData* bdWin = ArenaAllocType(a_arena, ImInputData);
	io.BackendPlatformUserData = reinterpret_cast<void*>(bdWin);
	io.BackendPlatformName = "imgui_impl_BB";
	io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;         // We can honor GetMouseCursor() values (optional)
	io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;          // We can honor io.WantSetMousePos requests (optional, rarely used)

	{ // WIN implementation

		bdWin->window = a_window;
		//bd->TicksPerSecond = perf_frequency;
		//bd->Time = perf_counter;
		bdWin->LastMouseCursor = ImGuiMouseCursor_COUNT;

		// Set platform dependent data in viewport
		ImGui::GetMainViewport()->PlatformHandleRaw = reinterpret_cast<void*>(a_window.handle);
	}
}

static inline void SetupImGuiRender(MemoryArena& a_arena)
{
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
		const uint32_t frame_count = GetBackBufferCount();
		bd->frame_index = 0;
		bd->frame_buffers = ArenaAllocArr(a_arena, ImRenderBuffer, frame_count);

		for (size_t i = 0; i < frame_count; i++)
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

	MemoryArenaScope(a_arena)
	{
		Asset::TextureLoadFromMemory load_image_memory;
		load_image_memory.name = "imgui font";
		load_image_memory.width = static_cast<uint32_t>(width);
		load_image_memory.height = static_cast<uint32_t>(height);
		load_image_memory.pixels = pixels;
		load_image_memory.bytes_per_pixel = 4;
		const Image& image = Asset::LoadImageMemory(a_arena, load_image_memory);
		bd->font = image.asset_handle;
		bd->font_descriptor = image.descriptor_index;
	}

	io.Fonts->SetTexID(bd->font_descriptor.handle);
}

inline static RPipelineLayout ImSetRenderState(const ImDrawData& a_draw_data, const RCommandList a_cmd_list, const uint32_t a_vert_pos, const ShaderEffectHandle* a_vertex_and_fragment)
{
	ImRenderData* bd = ImGetRenderData();

	const RPipelineLayout layout = BindShaders(a_cmd_list, ConstSlice<ShaderEffectHandle>(a_vertex_and_fragment, 2));

	{
		SetFrontFace(a_cmd_list, true);
		SetCullMode(a_cmd_list, CULL_MODE::NONE);
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

		SetPushConstants(a_cmd_list, layout, 0, sizeof(shader_indices), &shader_indices);
	}

	return layout;
}

inline static void ImGrowFrameBufferGPUBuffers(ImRenderBuffer& a_rb, const size_t a_new_vertex_size, const size_t a_new_index_size)
{
	// free I guess, lol can't do that now XDDD

	a_rb.vertex_buffer = AllocateFromWritableVertexBuffer(a_new_vertex_size);
	a_rb.index_buffer = AllocateFromWritableIndexBuffer(a_new_index_size);
}

bool BB::ImInit(MemoryArena& a_arena, const WindowHandle a_window_handle)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImPlot::CreateContext();
	ImGui::StyleColorsClassic();

	SetupImGuiInput(a_arena, a_window_handle);
	SetupImGuiRender(a_arena);

	const ImRenderData* bd = ImGetRenderData();
	const ImInputData* pd = ImGetInputData();

	return bd->font_descriptor.IsValid() && bd->font.IsValid() && pd->window.IsValid();
}

void BB::ImShutdown()
{
	ImRenderData* bd = ImGetRenderData();
	ImInputData* pd = ImGetInputData();
	BB_ASSERT(bd != nullptr, "No renderer backend to shutdown, or already shutdown?");
	BB_ASSERT(pd != nullptr, "No platform backend to shutdown, or already shutdown?");
	ImGuiIO& io = ImGui::GetIO();

	Asset::FreeAsset(bd->font);

	bd = {};
	pd = {};
	io = {};

	ImGui::DestroyContext();
}

void BB::ImNewFrame(const uint2 a_screen_extent)
{
	ImGuiIO& io = ImGui::GetIO();

	io.DisplaySize = ImVec2(
		static_cast<float>(a_screen_extent.x),
		static_cast<float>(a_screen_extent.y));

	ImGui::NewFrame();
}

void BB::ImRenderFrame(const RCommandList a_cmd_list, const RImageView a_render_target_view, const uint2 a_render_target_extent, const bool a_clear_image, const ShaderEffectHandle a_vertex, const ShaderEffectHandle a_fragment)
{
	ImGui::Render();
	const ImDrawData& draw_data = *ImGui::GetDrawData();
	// Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
	int fb_width = static_cast<int>(draw_data.DisplaySize.x * draw_data.FramebufferScale.x);
	int fb_height = static_cast<int>(draw_data.DisplaySize.y * draw_data.FramebufferScale.y);
	if (fb_width <= 0 || fb_height <= 0)
		return;

	ImRenderData* bd = ImGetRenderData();
	const uint32_t frame_count = GetBackBufferCount();

	BB_ASSERT(bd->frame_index < frame_count, "Frame index is higher then the framebuffer amount! Forgot to resize the imgui window info.");
	bd->frame_index = (bd->frame_index + 1) % frame_count;
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
	color_attach.image_layout = IMAGE_LAYOUT::RT_COLOR;
	color_attach.image_view = a_render_target_view;

	StartRenderingInfo imgui_pass_start{};
	imgui_pass_start.render_area_extent = a_render_target_extent;
	imgui_pass_start.render_area_offset = {};
	imgui_pass_start.color_attachments = Slice(&color_attach, 1);
	imgui_pass_start.depth_attachment = nullptr;
	StartRenderPass(a_cmd_list, imgui_pass_start);
	FixedArray<ColorBlendState, 1> blend_state;
	blend_state[0].blend_enable = true;
	blend_state[0].color_flags = 0xF;
	blend_state[0].color_blend_op = BLEND_OP::ADD;
	blend_state[0].src_blend = BLEND_MODE::FACTOR_SRC_ALPHA;
	blend_state[0].dst_blend = BLEND_MODE::FACTOR_ONE_MINUS_SRC_ALPHA;
	blend_state[0].alpha_blend_op = BLEND_OP::ADD;
	blend_state[0].src_alpha_blend = BLEND_MODE::FACTOR_ONE;
	blend_state[0].dst_alpha_blend = BLEND_MODE::FACTOR_ZERO;
	SetBlendMode(a_cmd_list, 0, blend_state.slice());

	// Setup desired CrossRenderer state
	const ShaderEffectHandle shader_effects[]{ a_vertex, a_fragment };
	const RPipelineLayout pipeline_layout = ImSetRenderState(draw_data, a_cmd_list, 0, shader_effects);

	// Will project scissor/clipping rectangles into framebuffer space
	const ImVec2 clip_off = draw_data.DisplayPos;    // (0,0) unless using multi-viewports
	const ImVec2 clip_scale = draw_data.FramebufferScale; // (1,1) unless using retina display which are often (2,2)

	// Because we merged all buffers into a single one, we maintain our own offset into them
	uint32_t global_idx_offset = 0;
	uint32_t vertex_offset = static_cast<uint32_t>(rb.vertex_buffer.offset);

	BindIndexBuffer(a_cmd_list, rb.index_buffer.offset, true);

	ImTextureID last_texture = bd->font_descriptor.handle;
	for (int n = 0; n < draw_data.CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data.CmdLists[n];
		SetPushConstants(a_cmd_list, pipeline_layout, IM_OFFSETOF(ShaderIndices2D, vertex_buffer_offset), sizeof(vertex_offset), &vertex_offset);

		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
		{
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
			if (pcmd->UserCallback != nullptr)
			{
				// User callback, registered via ImDrawList::AddCallback()
				// (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
				if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
					ImSetRenderState(draw_data, a_cmd_list, vertex_offset, shader_effects);
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
					SetPushConstants(a_cmd_list, pipeline_layout, IM_OFFSETOF(ShaderIndices2D, albedo_texture), sizeof(new_text), &new_text);
					last_texture = new_text;
				}
				// Apply scissor/clipping rectangle
				ScissorInfo scissor;
				scissor.offset.x = static_cast<int32_t>(clip_min.x);
				scissor.offset.y = static_cast<int32_t>(clip_min.y);
				scissor.extent.x = static_cast<uint32_t>(clip_max.x);
				scissor.extent.y = static_cast<uint32_t>(clip_max.y);
				SetScissor(a_cmd_list, scissor);

				// Draw
				const uint32_t index_offset = pcmd->IdxOffset + global_idx_offset;
				DrawIndexed(a_cmd_list, pcmd->ElemCount, 1, index_offset, 0, 0);
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
	SetScissor(a_cmd_list, base_scissor);

	EndRenderPass(a_cmd_list);
}

bool BB::ImProcessInput(const struct InputEvent& a_input_event)
{
	ImGuiIO& io = ImGui::GetIO();
	if (a_input_event.input_type == INPUT_TYPE::MOUSE)
	{
		const BB::MouseInfo& mouse_info = a_input_event.mouse_info;
		io.AddMousePosEvent(mouse_info.mouse_pos.x, mouse_info.mouse_pos.y);
		if (a_input_event.mouse_info.wheel_move != 0)
		{
			io.AddMouseWheelEvent(0.0f, static_cast<float>(a_input_event.mouse_info.wheel_move));
		}

		constexpr int left_button = 0;
		constexpr int right_button = 1;
		constexpr int middle_button = 2;

		if (mouse_info.left_pressed)
			io.AddMouseButtonEvent(left_button, true);
		if (mouse_info.right_pressed)
			io.AddMouseButtonEvent(right_button, true);
		if (mouse_info.middle_pressed)
			io.AddMouseButtonEvent(middle_button, true);

		if (mouse_info.left_released)
			io.AddMouseButtonEvent(left_button, false);
		if (mouse_info.right_released)
			io.AddMouseButtonEvent(right_button, false);
		if (mouse_info.middle_released)
			io.AddMouseButtonEvent(middle_button, false);

		return false;
	}
	else if (a_input_event.input_type == INPUT_TYPE::KEYBOARD)
	{
		const BB::KeyInfo& key_info = a_input_event.key_info;
		const ImGuiKey imgui_key = ImBBKeyToImGuiKey(key_info.scan_code);

		io.AddKeyEvent(imgui_key, key_info.key_pressed);
		io.AddInputCharacterUTF16(key_info.utf16);
		return false;
	}

	return false;
}
