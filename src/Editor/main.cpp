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

static float4x4 CalculateProjection(float2 a_extent)
{
	return Float4x4Perspective(ToRadians(60.0f), a_extent.x / a_extent.y, 0.001f, 10000.0f);
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

	ShaderEffectHandle shader_effects[3]{};
	MemoryArenaScope(main_arena)
	{
		CreateShaderEffectInfo shader_effect_create_infos[3];
		shader_effect_create_infos[0].name = "debug vertex shader";
		shader_effect_create_infos[0].stage = SHADER_STAGE::VERTEX;
		shader_effect_create_infos[0].next_stages = static_cast<uint32_t>(SHADER_STAGE::FRAGMENT_PIXEL);
		shader_effect_create_infos[0].shader_path = "../../resources/shaders/hlsl/Debug.hlsl";
		shader_effect_create_infos[0].shader_entry = "VertexMain";
		shader_effect_create_infos[0].push_constant_space = sizeof(ShaderIndices);
		shader_effect_create_infos[0].pass_type = RENDER_PASS_TYPE::STANDARD_3D;

		shader_effect_create_infos[1].name = "debug fragment shader";
		shader_effect_create_infos[1].stage = SHADER_STAGE::FRAGMENT_PIXEL;
		shader_effect_create_infos[1].next_stages = static_cast<uint32_t>(SHADER_STAGE::NONE);
		shader_effect_create_infos[1].shader_path = "../../resources/shaders/hlsl/Debug.hlsl";
		shader_effect_create_infos[1].shader_entry = "FragmentMain";
		shader_effect_create_infos[1].push_constant_space = sizeof(ShaderIndices);
		shader_effect_create_infos[1].pass_type = RENDER_PASS_TYPE::STANDARD_3D;

		shader_effect_create_infos[2].name = "jitter vertex shader";
		shader_effect_create_infos[2].stage = SHADER_STAGE::VERTEX;
		shader_effect_create_infos[2].next_stages = static_cast<uint32_t>(SHADER_STAGE::FRAGMENT_PIXEL);
		shader_effect_create_infos[2].shader_path = "../../resources/shaders/hlsl/Jitter.hlsl";
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
		async_assets[0].mesh_disk.path = "../../resources/models/Duck.gltf";
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

	auto current_time = std::chrono::high_resolution_clock::now();

	bool quit_app = false;

	while (!quit_app)
	{
		Asset::Update();
		
		auto currentnew = std::chrono::high_resolution_clock::now();
		delta_time = std::chrono::duration<float, std::chrono::seconds::period>(currentnew - current_time).count();
		current_time = currentnew;
	}

	DestroyImGuiInput();
	DirectDestroyOSWindow(window);

	return 0;
}
