#include "Editor.hpp"

#include "Program.h"
#include "BBThreadScheduler.hpp"
#include "HID.h"
#include "MemoryArena.hpp"
#include "math.inl"

#include "Renderer.hpp"
#include "imgui.h"

#include "Camera.hpp"
#include "SceneHierarchy.hpp"

using namespace BB;

struct ImInputData
{
	BB::WindowHandle            window;
	int                         MouseTrackedArea;   // 0: not tracked, 1: client are, 2: non-client area
	int                         MouseButtonsDown;
	int64_t                     Time;
	int64_t                     TicksPerSecond;
	ImGuiMouseCursor            LastMouseCursor;

	ImInputData() { memset(this, 0, sizeof(*this)); }
};

static ImInputData* ImGui_ImplBB_GetPlatformData()
{
	return ImGui::GetCurrentContext() ? reinterpret_cast<ImInputData*>(ImGui::GetIO().BackendPlatformUserData) : nullptr;
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

//On true means that imgui takes the input and doesn't give it to the engine.
static bool ImProcessInput(const BB::InputEvent& a_input_event)
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

static inline void DestroyImGuiInput()
{
	ImGuiIO& io = ImGui::GetIO();
	ImInputData* pd = ImGui_ImplBB_GetPlatformData();
	BB_ASSERT(pd != nullptr, "No platform backend to shutdown, or already shutdown?");

	io.BackendPlatformName = nullptr;
	io.BackendPlatformUserData = nullptr;
	IM_DELETE(pd);
}

static float4x4 CalculateProjection(float2 a_extent)
{
	return Float4x4Perspective(ToRadians(60.0f), a_extent.x / a_extent.y, 0.001f, 10000.0f);
}

static Viewport CreateViewport(MemoryArena& a_arena, const uint2 a_extent, const uint2 a_offset, const char* a_name)
{
	Viewport viewport{};
	viewport.extent = a_extent;
	viewport.offset = a_offset;
	viewport.render_target = CreateRenderTarget(a_arena, a_extent, a_name);
	viewport.name = a_name;
	return viewport;
}

static void ViewportResize(Viewport& a_viewport, const uint2 a_new_extent)
{
	if (a_viewport.extent == a_new_extent)
		return;

	a_viewport.extent = a_new_extent;
	ResizeRenderTarget(a_viewport.render_target, a_new_extent);
}

static void DrawImGuiViewport(Viewport& a_viewport, bool& a_resized, const uint2 a_minimum_size = uint2(160, 80))
{
	a_resized = false;
	if (ImGui::Begin(a_viewport.name, nullptr, ImGuiWindowFlags_MenuBar))
	{
		const RTexture render_target = GetCurrentRenderTargetTexture(a_viewport.render_target);
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("screenshot"))
			{
				static char image_name[128]{};
				ImGui::InputText("sceenshot name", image_name, 128);

				if (ImGui::Button("make screenshot"))
				{
					// just hard stall, this is a button anyway

					CommandPool& pool = GetGraphicsCommandPool();
					const RCommandList list = pool.StartCommandList();

					GPUBufferCreateInfo readback_info;
					readback_info.name = "viewport screenshot readback";
					readback_info.size = static_cast<uint64_t>(a_viewport.extent.x * a_viewport.extent.y * 4u);
					readback_info.type = BUFFER_TYPE::READBACK;
					readback_info.host_writable = true;
					GPUBuffer readback = CreateGPUBuffer(readback_info);

					ReadTexture(list, render_target, a_viewport.extent, int2(0, 0), readback, readback_info.size);

					pool.EndCommandList(list);
					uint64_t fence;
					BB_ASSERT(ExecuteGraphicCommands(Slice(&pool, 1), fence), "Failed to make a screenshot");

					StackString<256> image_name_bmp{ "screenshots" };
					if (OSDirectoryExist(image_name_bmp.c_str()))
						OSCreateDirectory(image_name_bmp.c_str());

					image_name_bmp.push_back('/');
					image_name_bmp.append(image_name);
					image_name_bmp.append(".png");

					// maybe deadlock if it's never idle...
					GPUWaitIdle();

					const void* readback_mem = MapGPUBuffer(readback);

					BB_ASSERT(Asset::WriteImage(image_name_bmp.c_str(), a_viewport.extent.x, a_viewport.extent.y, 4, readback_mem), "failed to write screenshot image to disk");

					UnmapGPUBuffer(readback);
					FreeGPUBuffer(readback);
				}

				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}


		ImGuiIO im_io = ImGui::GetIO();

		const ImVec2 window_offset = ImGui::GetWindowPos();
		a_viewport.offset = uint2(static_cast<unsigned int>(window_offset.x), static_cast<unsigned int>(window_offset.y));

		if (static_cast<unsigned int>(ImGui::GetWindowSize().x) < a_minimum_size.x ||
			static_cast<unsigned int>(ImGui::GetWindowSize().y) < a_minimum_size.y)
		{
			ImGui::SetWindowSize(ImVec2(static_cast<float>(a_minimum_size.x), static_cast<float>(a_minimum_size.y)));
			ImGui::End();
			return;
		}

		const ImVec2 viewport_draw_area = ImGui::GetContentRegionAvail();

		const uint2 window_size_u = uint2(static_cast<unsigned int>(viewport_draw_area.x), static_cast<unsigned int>(viewport_draw_area.y));
		if (window_size_u != a_viewport.extent && !im_io.WantCaptureMouse)
		{
			a_resized = true;
			ViewportResize(a_viewport, window_size_u);
		}

		ImGui::Image(render_target.handle, viewport_draw_area);
	}
	ImGui::End();
}

static bool PositionWithinViewport(const Viewport& a_viewport, const uint2 a_pos)
{
	if (a_viewport.offset.x < a_pos.x &&
		a_viewport.offset.y < a_pos.y &&
		a_viewport.offset.x + a_viewport.extent.x > a_pos.x &&
		a_viewport.offset.y + a_viewport.extent.y > a_pos.y)
		return true;
	return false;
}

static void MainDebugWindow(const MemoryArena& a_arena, const Viewport* a_selected_viewport)
{
	if (ImGui::Begin("general engine info"))
	{
		if (a_selected_viewport == nullptr)
			ImGui::Text("Current viewport: None");
		else
			ImGui::Text("Current viewport: %s", a_selected_viewport->name);

		if (ImGui::CollapsingHeader("main allocator"))
		{
			ImGui::Indent();
			ImGui::Text("standard reserved memory: %zu", ARENA_DEFAULT_RESERVE);
			ImGui::Text("commit size %zu", ARENA_DEFAULT_COMMIT);

			if (ImGui::CollapsingHeader("main stack memory arena"))
			{
				ImGui::Indent();
				const size_t remaining = MemoryArenaSizeRemaining(a_arena);
				const size_t commited = MemoryArenaSizeCommited(a_arena);
				const size_t used = MemoryArenaSizeUsed(a_arena);

				ImGui::Text("memory remaining: %zu", remaining);
				ImGui::Text("memory commited: %zu", commited);
				ImGui::Text("memory used: %zu", used);

				const float perc_calculation = 1.f / static_cast<float>(ARENA_DEFAULT_COMMIT);
				ImGui::Separator();
				ImGui::TextUnformatted("memory used till next commit");
				ImGui::ProgressBar(perc_calculation * static_cast<float>(RoundUp(used, ARENA_DEFAULT_COMMIT) - used));
				ImGui::Unindent();
			}
		}
	}
	ImGui::End();
}

struct ThreadFuncForDrawing_Params
{
	//IN
	Viewport& viewport;
	SceneHierarchy& scene_hierarchy;
	RCommandList command_list;
};

static void ThreadFuncForDrawing(void* a_param)
{
	ThreadFuncForDrawing_Params* param_in = reinterpret_cast<ThreadFuncForDrawing_Params*>(a_param);

	Viewport& viewport = param_in->viewport;
	SceneHierarchy& scene_hierarchy = param_in->scene_hierarchy;
	RCommandList list = param_in->command_list;

	StartRenderTarget(list, viewport.render_target);

	scene_hierarchy.DrawSceneHierarchy(list, viewport.render_target, viewport.extent, int2(0, 0));

	EndRenderTarget(list, viewport.render_target);
}

void Editor::Init(struct BB::MemoryArena& a_arena, const uint2 a_window_extent)
{
	m_main_window = CreateOSWindow(
		BB::OS_WINDOW_STYLE::MAIN,
		static_cast<int>(a_window_extent.x) / 4,
		static_cast<int>(a_window_extent.y) / 4,
		static_cast<int>(a_window_extent.x),
		static_cast<int>(a_window_extent.y),
		L"Modern Vulkan - editor");

	RendererCreateInfo render_create_info;
	render_create_info.app_name = "modern vulkan - editor";
	render_create_info.engine_name = "building block engine - editor";
	render_create_info.window_handle = m_main_window;
	render_create_info.swapchain_width = a_window_extent.x;
	render_create_info.swapchain_height = a_window_extent.y;
	render_create_info.debug = true;
	InitializeRenderer(a_arena, render_create_info);

	SetupImGuiInput(a_arena, m_main_window);

	m_game_screen = CreateViewport(a_arena, a_window_extent, uint2(), "game scene");
	m_object_viewer_screen = CreateViewport(a_arena, a_window_extent / 2u, uint2(), "object viewer");

	m_game_hierarchy.InitViaJson(a_arena, "../../resources/scenes/standard_scene.json");
	m_object_viewer_hierarchy.InitViaJson(a_arena, "../../resources/scenes/standard_scene.json");
	m_game_hierarchy.SetClearColor(float3{ 0.1f, 0.6f, 0.1f });
	m_object_viewer_hierarchy.SetClearColor(float3{ 0.5f, 0.1f, 0.1f });
}

void Editor::Destroy()
{
	DestroyImGuiInput();
	DirectDestroyOSWindow(m_main_window);
}

void Editor::Update(MemoryArena& a_arena, const float a_delta_time)
{
	InputEvent input_events[INPUT_EVENT_BUFFER_MAX]{};
	size_t input_event_count = 0;

	ProcessMessages(m_main_window);
	PollInputEvents(input_events, input_event_count);

	for (size_t i = 0; i < input_event_count; i++)
	{
		const InputEvent& ip = input_events[i];
		//imgui can deny our normal input
		if (ImProcessInput(ip))
			continue;

		if (ip.input_type == INPUT_TYPE::KEYBOARD)
		{
			const KeyInfo& ki = ip.key_info;
			float3 cam_move{};
			if (ki.key_pressed)
				switch (ki.scan_code)
				{
				case KEYBOARD_KEY::F:
					m_freeze_cam = !m_freeze_cam;
					break;
				case KEYBOARD_KEY::W:
					cam_move.y = 1;
					break;
				case KEYBOARD_KEY::S:
					cam_move.y = -1;
					break;
				case KEYBOARD_KEY::A:
					cam_move.x = 1;
					break;
				case KEYBOARD_KEY::D:
					cam_move.x = -1;
					break;
				case KEYBOARD_KEY::X:
					cam_move.z = 1;
					break;
				case KEYBOARD_KEY::Z:
					cam_move.z = -1;
					break;
				default:
					break;
				}
			if (!m_freeze_cam && m_active_viewport)
			{
				m_active_viewport->camera.Move(cam_move);
			}

		}
		else if (ip.input_type == INPUT_TYPE::MOUSE)
		{
			const MouseInfo& mi = ip.mouse_info;
			const float2 mouse_move = (mi.move_offset * a_delta_time) * 10.f;
			m_previous_mouse_pos = mi.mouse_pos;

			if (mi.right_released)
				FreezeMouseOnWindow(m_main_window);
			if (mi.left_released)
				UnfreezeMouseOnWindow();

			if (PositionWithinViewport(m_game_screen, uint2(static_cast<unsigned int>(mi.mouse_pos.x), static_cast<unsigned int>(mi.mouse_pos.y))))
			{
				m_active_viewport = &m_game_screen;
			}
			else if (PositionWithinViewport(m_object_viewer_screen, uint2(static_cast<unsigned int>(mi.mouse_pos.x), static_cast<unsigned int>(mi.mouse_pos.y))))
			{
				m_active_viewport = &m_object_viewer_screen;
			}

			if (!m_freeze_cam && m_active_viewport)
			{
				m_active_viewport->camera.Rotate(mouse_move.x, mouse_move.y);
			}
		}
	}

	CommandPool graphics_command_pools[2]{ GetGraphicsCommandPool(), GetGraphicsCommandPool() };
	const RCommandList main_list = graphics_command_pools[0].StartCommandList();

	StartFrameInfo start_info;
	start_info.delta_time = a_delta_time;
	start_info.mouse_pos = m_previous_mouse_pos;

	StartFrame(main_list, start_info);

	MemoryArenaScope(a_arena)
	{
		m_game_hierarchy.SetView(m_game_screen.camera.CalculateView());
		m_object_viewer_hierarchy.SetView(m_object_viewer_screen.camera.CalculateView());

		Asset::ShowAssetMenu(a_arena);
		MainDebugWindow(a_arena, m_active_viewport);
		m_game_hierarchy.ImguiDisplaySceneHierarchy();
		m_object_viewer_hierarchy.ImguiDisplaySceneHierarchy();

		bool resized = false;
		DrawImGuiViewport(m_game_screen, resized);
		if (resized)
		{
			m_game_hierarchy.SetProjection(CalculateProjection(float2(static_cast<float>(m_game_screen.extent.x), static_cast<float>(m_game_screen.extent.y))));
		}

		resized = false;
		DrawImGuiViewport(m_object_viewer_screen, resized);
		if (resized)
		{
			m_object_viewer_hierarchy.SetProjection(CalculateProjection(float2(static_cast<float>(m_object_viewer_screen.extent.x), static_cast<float>(m_object_viewer_screen.extent.y))));
		}

		ThreadFuncForDrawing_Params main_scene_params{
			m_game_screen,
			m_game_hierarchy,
			main_list
		};

		const RCommandList object_viewer_list = graphics_command_pools[1].StartCommandList();

		ThreadFuncForDrawing_Params object_viewer_params{
			m_object_viewer_screen,
			m_object_viewer_hierarchy,
			object_viewer_list
		};

		ThreadTask main_scene_task = Threads::StartTaskThread(ThreadFuncForDrawing, &main_scene_params, L"main scene task");
		ThreadTask object_viewer_task = Threads::StartTaskThread(ThreadFuncForDrawing, &object_viewer_params, L"object viewer task");

		Threads::WaitForTask(main_scene_task);
		Threads::WaitForTask(object_viewer_task);

		graphics_command_pools[1].EndCommandList(object_viewer_list);

		EndFrame(main_list);

		graphics_command_pools[0].EndCommandList(main_list);
		uint64_t fence_value;
		PresentFrame(Slice(graphics_command_pools, _countof(graphics_command_pools)), fence_value);
	}
}
