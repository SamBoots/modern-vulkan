//entry for the vulkan renderer
#include "BBMain.h"
#include "BBMemory.h"
#include "Program.h"
#include "BBThreadScheduler.hpp"
#include "HID.h"

#include "Camera.hpp"
#include "Transform.hpp"
#include "AssetLoader.hpp"

#include <chrono>

#include "shared_common.hlsl.h"
#include "Renderer.hpp"

#include "Math.inl"


using namespace BB;

static void DrawglTFNode(Model::Node& a_node, const float4x4& a_transform)
{
	float4x4 local_transform = a_transform * a_node.transform;

	if (a_node.mesh_handle.handle != BB::BB_INVALID_HANDLE_64)
		for (uint32_t i = 0; i < a_node.primitive_count; i++)
			DrawMesh(a_node.mesh_handle, local_transform, a_node.primitives[i].start_index, a_node.primitives[i].index_count, a_node.primitives[i].material);

	for (size_t i = 0; i < a_node.child_count; i++)
	{
		DrawglTFNode(a_node.childeren[i], local_transform);
	}
}

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

		return io.WantCaptureMouse;
	}
	else if (a_input_event.input_type == INPUT_TYPE::KEYBOARD)
	{
		const BB::KeyInfo& key_info = a_input_event.key_info;
		const ImGuiKey imgui_key = ImBBKeyToImGuiKey(key_info.scan_code);

		io.AddKeyEvent(imgui_key, key_info.key_pressed);
		//THIS IS WRONG! It gives no UTF16 character.
		//But i'll keep it in here to test if imgui input actually works.
		io.AddInputCharacterUTF16(static_cast<ImWchar16>(key_info.scan_code));

		return io.WantCaptureKeyboard;
	}

	return false;
}

static inline void SetupImGuiInput(StackAllocator_t& a_stack_allocator, const BB::WindowHandle a_window)
{
	ImGuiIO& io = ImGui::GetIO();

	// Setup backend capabilities flags
	ImInputData* bdWin = BBnew(a_stack_allocator, ImInputData);
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

int main(int argc, char** argv)
{
	(void)argc;

	BBInitInfo bb_init;
	bb_init.exePath = argv[0];
	bb_init.programName = L"Modern Vulkan";
	InitBB(bb_init);

	SystemInfo sys_info;
	OSSystemInfo(sys_info);
	Threads::InitThreads(sys_info.processor_num);

	StackAllocator_t main_allocator{ mbSize * 32 };

	int window_width = 1280;
	int window_height = 720;
	const WindowHandle window = CreateOSWindow(
		BB::OS_WINDOW_STYLE::MAIN,
		250,
		200,
		window_width,
		window_height,
		L"Modern Vulkan");

	RendererCreateInfo render_create_info;
	render_create_info.app_name = "modern vulkan";
	render_create_info.engine_name = "Building Block Engine";
	render_create_info.window_handle = window;
	render_create_info.swapchain_width = static_cast<uint32_t>(window_width);
	render_create_info.swapchain_height = static_cast<uint32_t>(window_height);
	render_create_info.debug = true;
	InitializeRenderer(main_allocator, render_create_info);
	SetupImGuiInput(main_allocator, window);
	Camera camera{ float3{2.0f, 2.0f, 2.0f}, 0.35f };

	TransformPool transform_pool{ main_allocator, 32 };
	const TransformHandle transform_test = transform_pool.CreateTransform(float3{ 0, -1, 1 });
	const TransformHandle transform_gltf = transform_pool.CreateTransform(float3{ 0, 1, 1 });

	{
		const float4x4 t_ProjMat = Float4x4Perspective(ToRadians(60.0f),
			static_cast<float>(render_create_info.swapchain_width) / static_cast<float>(render_create_info.swapchain_height),
			.001f, 10000.0f);
		BB::SetProjection(t_ProjMat);
	}

	ShaderEffectHandle shader_effects[2];
	BBStackAllocatorScope(main_allocator)
	{
		CreateShaderEffectInfo shader_effect_create_infos[2];
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

		BB_ASSERT(CreateShaderEffect(main_allocator,
			Slice(shader_effect_create_infos, _countof(shader_effect_create_infos)),
			shader_effects), "Failed to create shader objects");
	}

	//create material
	MaterialHandle default_mat;
	{
		CreateMaterialInfo material_info{};
		material_info.base_color = GetWhiteTexture();
		material_info.shader_effects = Slice(shader_effects, _countof(shader_effects));
		default_mat = CreateMaterial(material_info);
	}

	const Model* quad_mesh = nullptr;
	const Model* gltf_model = nullptr;
	BBStackAllocatorScope(main_allocator)
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
		async_assets[0].mesh_disk.name = "duck gltf";
		async_assets[0].mesh_disk.path = "../resources/models/Duck.gltf";
		async_assets[0].mesh_disk.shader_effects = Slice(shader_effects, _countof(shader_effects));

		async_assets[1].asset_type = Asset::ASYNC_ASSET_TYPE::MODEL;
		async_assets[1].load_type = Asset::ASYNC_LOAD_TYPE::MEMORY;
		async_assets[1].mesh_memory.name = "basic quad";
		async_assets[1].mesh_memory.vertices = Slice(vertices, _countof(vertices));
		async_assets[1].mesh_memory.indices = Slice(indices, _countof(indices));
		async_assets[1].mesh_memory.shader_effects = Slice(shader_effects, _countof(shader_effects));
		Asset::LoadASync(Slice(async_assets, _countof(async_assets)));

		gltf_model = Asset::FindModelByPath(async_assets[0].mesh_disk.path);
		quad_mesh = Asset::FindModelByName(async_assets[1].mesh_memory.name);
	}

	bool freeze_cam = false;
	bool quit_app = false;
	float delta_time = 0;

	static auto start_time = std::chrono::high_resolution_clock::now();
	auto current_time = std::chrono::high_resolution_clock::now();

	InputEvent input_events[INPUT_EVENT_BUFFER_MAX]{};
	size_t input_event_count = 0;

	while (!quit_app)
	{
		ProcessMessages(window);
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
				camera.Move(cam_move);
			}
			else if (ip.input_type == INPUT_TYPE::MOUSE)
			{
				const MouseInfo& mi = ip.mouse_info;
				const float2 mouse_move = (mi.move_offset * delta_time) * 0.001f;
				if (!freeze_cam)
					camera.Rotate(mouse_move.x, mouse_move.y);

				if (mi.right_released)
					FreezeMouseOnWindow(window);
				if (mi.left_released)
					UnfreezeMouseOnWindow();
			}
		}
		
		BBStackAllocatorScope(main_allocator)
		{
			BB::SetView(camera.CalculateView());
			StartFrame();


			for (size_t i = 0; i < gltf_model->root_node_count; i++)
			{
				const float4x4 root_matrix = transform_pool.GetTransform(transform_gltf).CreateMatrix();
				DrawglTFNode(gltf_model->root_nodes[i], root_matrix);
			}
			{
				const float4x4 root_matrix = transform_pool.GetTransform(transform_test).CreateMatrix();
				DrawMesh(quad_mesh->root_nodes[0].mesh_handle, root_matrix, quad_mesh->primitives[0].start_index, quad_mesh->primitives[0].index_count, default_mat);
			}


			EndFrame();
		}

		delta_time = std::chrono::duration<float, std::chrono::seconds::period>(current_time - start_time).count();
		current_time = std::chrono::high_resolution_clock::now();
	}

	DirectDestroyOSWindow(window);

	return 0;
}
