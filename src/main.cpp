//entry for the vulkan renderer
#include "BBMain.h"
#include "BBMemory.h"
#include "Program.h"
#include "BBThreadScheduler.hpp"
#include "HID.h"

#include "Camera.hpp"
#include "SceneHierarchy.hpp"

#include <chrono>

#include "shared_common.hlsl.h"

#include "Math.inl"

#include "Storage/FixedArray.h"

using namespace BB;
#include "imgui.h"

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

struct Viewport
{
	uint2 extent;
	uint2 offset; // offset into main window NOT USED NOW 
	RenderTarget render_target;
	const char* name;
	Camera camera{ float3{0.0f, 0.0f, 1.0f}, 0.35f };
};

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

static float4x4 CalculateProjection(float2 a_extent)
{
	return Float4x4Perspective(ToRadians(60.0f), a_extent.x / a_extent.y, 0.001f, 10000.0f);
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

static void CustomCloseWindow(const BB::WindowHandle a_window_handle)
{
	(void)a_window_handle;
	BB_ASSERT(false, "unimplemented");
}

static void CustomResizeWindow(const BB::WindowHandle a_window_handle, const uint32_t a_x, const uint32_t a_y)
{
	(void)a_x;
	(void)a_y;
	(void)a_window_handle;
	BB::RequestResize();
}

int main(int argc, char** argv)
{
	(void)argc;

	StackString<512> exe_path;

	{
		const StringView exe_path_manipulator{ argv[0] };
		const size_t path_end = exe_path_manipulator.find_last_of('\\');

		exe_path.append(exe_path_manipulator.c_str(), path_end);
	}

	
	BBInitInfo bb_init{};
	bb_init.exe_path = exe_path.c_str();
	bb_init.program_name = L"Modern Vulkan";
	InitBB(bb_init);

	SystemInfo sys_info;
	OSSystemInfo(sys_info);
	Threads::InitThreads(sys_info.processor_num / 2);

	MemoryArena main_arena = MemoryArenaCreate();

	uint2 window_extent = { 1280, 720 };
	const WindowHandle window = CreateOSWindow(
		BB::OS_WINDOW_STYLE::MAIN,
		static_cast<int>(window_extent.x) / 4,
		static_cast<int>(window_extent.y) / 4,
		static_cast<int>(window_extent.x),
		static_cast<int>(window_extent.y),
		L"Modern Vulkan");

	RendererCreateInfo render_create_info;
	render_create_info.app_name = "modern vulkan";
	render_create_info.engine_name = "building block engine";
	render_create_info.window_handle = window;
	render_create_info.swapchain_width = window_extent.x;
	render_create_info.swapchain_height = window_extent.y;
	render_create_info.debug = true;
	InitializeRenderer(main_arena, render_create_info);

	{
		const Asset::AssetManagerInitInfo asset_manager_info = {};
		Asset::InitializeAssetManager(asset_manager_info);
	}

	SetupImGuiInput(main_arena, window);

	SetWindowCloseEvent(CustomCloseWindow);
	SetWindowResizeEvent(CustomResizeWindow);

	SceneHierarchy scene_hierarchy;
	SceneHierarchy object_viewer_scene;
	Viewport viewport_scene;
	Viewport viewport_object_viewer;
	scene_hierarchy.InitializeSceneHierarchy(main_arena, 128, "normal scene");
	object_viewer_scene.InitializeSceneHierarchy(main_arena, 16, "object viewer scene");

	{
		CommandPool& startup_pool = GetGraphicsCommandPool();
		const RCommandList startup_list = startup_pool.StartCommandList();
		viewport_scene = CreateViewport(main_arena, window_extent, uint2(), "game scene");
		viewport_object_viewer = CreateViewport(main_arena, window_extent / 2u, uint2(), "object viewer");
		startup_pool.EndCommandList(startup_list);
		uint64_t mock_fence_value;
		ExecuteGraphicCommands(Slice(&startup_pool, 1), mock_fence_value);
	}

	scene_hierarchy.SetClearColor(float3{ 0.1f, 0.6f, 0.1f });
	object_viewer_scene.SetClearColor(float3{ 0.5f, 0.1f, 0.1f });

	ShaderEffectHandle shader_effects[3]{};
	MemoryArenaScope(main_arena)
	{
		CreateShaderEffectInfo shader_effect_create_infos[3];
		shader_effect_create_infos[0].name = "debug vertex shader";
		shader_effect_create_infos[0].stage = SHADER_STAGE::VERTEX;
		shader_effect_create_infos[0].next_stages = static_cast<uint32_t>(SHADER_STAGE::FRAGMENT_PIXEL);
		shader_effect_create_infos[0].shader_path = "../resources/shaders/hlsl/Debug.hlsl";
		shader_effect_create_infos[0].shader_entry = "VertexMain";
		shader_effect_create_infos[0].push_constant_space = sizeof(ShaderIndices);
		shader_effect_create_infos[0].pass_type = RENDER_PASS_TYPE::STANDARD_3D;

		shader_effect_create_infos[1].name = "debug fragment shader";
		shader_effect_create_infos[1].stage = SHADER_STAGE::FRAGMENT_PIXEL;
		shader_effect_create_infos[1].next_stages = static_cast<uint32_t>(SHADER_STAGE::NONE);
		shader_effect_create_infos[1].shader_path = "../resources/shaders/hlsl/Debug.hlsl";
		shader_effect_create_infos[1].shader_entry = "FragmentMain";
		shader_effect_create_infos[1].push_constant_space = sizeof(ShaderIndices);
		shader_effect_create_infos[1].pass_type = RENDER_PASS_TYPE::STANDARD_3D;

		shader_effect_create_infos[2].name = "jitter vertex shader";
		shader_effect_create_infos[2].stage = SHADER_STAGE::VERTEX;
		shader_effect_create_infos[2].next_stages = static_cast<uint32_t>(SHADER_STAGE::FRAGMENT_PIXEL);
		shader_effect_create_infos[2].shader_path = "../resources/shaders/hlsl/Jitter.hlsl";
		shader_effect_create_infos[2].shader_entry = "VertexMain";
		shader_effect_create_infos[2].push_constant_space = sizeof(ShaderIndices);
		shader_effect_create_infos[2].pass_type = RENDER_PASS_TYPE::STANDARD_3D;

		BB_ASSERT(CreateShaderEffect(main_arena,
			Slice(shader_effect_create_infos, _countof(shader_effect_create_infos)),
			shader_effects), "Failed to create shader objects");
	}

	//create material
	MaterialHandle default_mat;
	{
		CreateMaterialInfo material_info{};
		material_info.name = "base material";
		material_info.base_color = GetWhiteTexture();
		material_info.shader_effects = Slice(shader_effects, 2);
		default_mat = CreateMaterial(material_info);
	}

	MemoryArenaScope(main_arena)
	{
		//Do some simpel model loading and drawing.
		Vertex vertices[4];
		vertices[0] = { {-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f} };
		vertices[1] = { {0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f} };
		vertices[2] = { {0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f} };
		vertices[3] = { {-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 1.0f, 1.0f} };

		uint32_t indices[] = { 0, 1, 2, 2, 3, 0 };


		Asset::AsyncAsset async_assets[2]{};
		async_assets[0].asset_type = Asset::ASYNC_ASSET_TYPE::MODEL;
		async_assets[0].load_type = Asset::ASYNC_LOAD_TYPE::DISK;
		async_assets[0].mesh_disk.path = "../resources/models/Duck.gltf";
		async_assets[0].mesh_disk.shader_effects = Slice(shader_effects, 2);

		//async_assets[1].asset_type = Asset::ASYNC_ASSET_TYPE::MODEL;
		//async_assets[1].load_type = Asset::ASYNC_LOAD_TYPE::DISK;
		//async_assets[1].mesh_disk.name = "Sponza gltf";
		//async_assets[1].mesh_disk.path = "../resources/models/Sponza.gltf";
		//async_assets[1].mesh_disk.shader_effects = Slice(shader_effects, _countof(shader_effects));

		async_assets[1].asset_type = Asset::ASYNC_ASSET_TYPE::MODEL;
		async_assets[1].load_type = Asset::ASYNC_LOAD_TYPE::MEMORY;
		async_assets[1].mesh_memory.name = "basic quad";
		async_assets[1].mesh_memory.vertices = Slice(vertices, _countof(vertices));
		async_assets[1].mesh_memory.indices = Slice(indices, _countof(indices));
		async_assets[1].mesh_memory.material = default_mat;
		ThreadTask asset_job = Asset::LoadAssetsASync(Slice(async_assets, _countof(async_assets)));

		Threads::WaitForTask(asset_job);

		//scene_hierarchy.CreateSceneObjectViaModel(*Asset::FindModelByPath(async_assets[1].mesh_disk.path), float3{ 0, -2, 3 }, "sponzay");
		scene_hierarchy.CreateSceneObjectViaModel(*Asset::FindModelByPath(async_assets[0].mesh_disk.path), float3{ 0, -1, 1 }, "ducky");
		object_viewer_scene.CreateSceneObjectViaModel(*Asset::FindModelByPath(async_assets[0].mesh_disk.path), float3{ 0, -2, 3 }, "ducky");
	}

	{	// add some basic lights
		BB::FixedArray<CreateLightInfo, 2> light_create_info;
		light_create_info[0].color = float3(1, 1, 1);
		light_create_info[0].linear_distance = 0.35f;
		light_create_info[0].quadratic_distance = 0.44f;
		light_create_info[0].pos = float3(3, 0, 0);

		light_create_info[1].color = float3(1, 1, 1);
		light_create_info[1].linear_distance = 0.35f;
		light_create_info[1].quadratic_distance = 0.44f;
		light_create_info[1].pos = float3(0, 4, 0);

		scene_hierarchy.CreateSceneObjectAsLight(light_create_info[0], "light 0");
		scene_hierarchy.CreateSceneObjectAsLight(light_create_info[1], "light 1");

		object_viewer_scene.CreateSceneObjectAsLight(light_create_info[0], "light 0");
	}

	scene_hierarchy.SetView(viewport_scene.camera.CalculateView());
	object_viewer_scene.SetView(viewport_object_viewer.camera.CalculateView());

	bool freeze_cam = false;
	bool quit_app = false;

	auto current_time = std::chrono::high_resolution_clock::now();
	float delta_time = 0;

	InputEvent input_events[INPUT_EVENT_BUFFER_MAX]{};
	size_t input_event_count = 0;

	//jank? Yeah. Make it a system
	Viewport* active_viewport = nullptr;
	float2 previous_mouse_pos{};

	while (!quit_app)
	{
		ProcessMessages(window);
		PollInputEvents(input_events, input_event_count);

		CommandPool graphics_command_pools[2]{ GetGraphicsCommandPool(), GetGraphicsCommandPool() };
		const RCommandList main_list = graphics_command_pools[0].StartCommandList();

		StartFrameInfo start_info;
		start_info.delta_time = delta_time;
		start_info.mouse_pos = previous_mouse_pos;

		StartFrame(main_list, start_info);

		Asset::Update();

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
						freeze_cam = !freeze_cam;
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
				if (!freeze_cam && active_viewport)
				{
					active_viewport->camera.Move(cam_move);
				}

			}
			else if (ip.input_type == INPUT_TYPE::MOUSE)
			{
				const MouseInfo& mi = ip.mouse_info;
				const float2 mouse_move = (mi.move_offset * delta_time) * 10.f;
				previous_mouse_pos = mi.mouse_pos;

				if (mi.right_released)
					FreezeMouseOnWindow(window);
				if (mi.left_released)
					UnfreezeMouseOnWindow();

				{
					Viewport* new_active = nullptr;
					if (PositionWithinViewport(viewport_scene, uint2(static_cast<unsigned int>(mi.mouse_pos.x), static_cast<unsigned int>(mi.mouse_pos.y))))
					{
						new_active = &viewport_scene;
					}
					if (PositionWithinViewport(viewport_object_viewer, uint2(static_cast<unsigned int>(mi.mouse_pos.x), static_cast<unsigned int>(mi.mouse_pos.y))))
					{
						new_active = &viewport_object_viewer;
					}
					active_viewport = new_active;
				}

				if (!freeze_cam && active_viewport)
					active_viewport->camera.Rotate(mouse_move.x, mouse_move.y);
			}
		}
		MemoryArenaScope(main_arena)
		{

			scene_hierarchy.SetView(viewport_scene.camera.CalculateView());
			object_viewer_scene.SetView(viewport_object_viewer.camera.CalculateView());

			Asset::ShowAssetMenu(main_arena);
			MainDebugWindow(main_arena, active_viewport);
			scene_hierarchy.ImguiDisplaySceneHierarchy();
			object_viewer_scene.ImguiDisplaySceneHierarchy();

			bool resized = false;
			DrawImGuiViewport(viewport_scene, resized);
			if (resized)
			{
				scene_hierarchy.SetProjection(CalculateProjection(float2(static_cast<float>(viewport_scene.extent.x), static_cast<float>(viewport_scene.extent.y))));
			}

			resized = false;
			DrawImGuiViewport(viewport_object_viewer, resized);
			if (resized)
			{
				object_viewer_scene.SetProjection(CalculateProjection(float2(static_cast<float>(viewport_object_viewer.extent.x), static_cast<float>(viewport_object_viewer.extent.y))));
			}

			ThreadFuncForDrawing_Params main_scene_params{
				viewport_scene,
				scene_hierarchy,
				main_list
			};

			const RCommandList object_viewer_list = graphics_command_pools[1].StartCommandList();

			ThreadFuncForDrawing_Params object_viewer_params{
				viewport_object_viewer,
				object_viewer_scene,
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

		auto currentnew = std::chrono::high_resolution_clock::now();
		delta_time = std::chrono::duration<float, std::chrono::seconds::period>(currentnew - current_time).count();
		current_time = currentnew;
	}

	DestroyImGuiInput();
	DirectDestroyOSWindow(window);

	return 0;
}
