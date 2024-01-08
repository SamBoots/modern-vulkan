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

#include "MemoryArena.hpp"

#include "Math.inl"

#include "Storage/FixedArray.h"
#include "Storage/Slotmap.h"


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

		return io.WantCaptureMouse;
	}
	else if (a_input_event.input_type == INPUT_TYPE::KEYBOARD)
	{
		const BB::KeyInfo& key_info = a_input_event.key_info;
		const ImGuiKey imgui_key = ImBBKeyToImGuiKey(key_info.scan_code);

		io.AddKeyEvent(imgui_key, key_info.key_pressed);

		return io.WantCaptureKeyboard;
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

static void DebugWindowMemoryArena(const MemoryArena& a_arena)
{
	const MemoryArenaAllocationInfo* a_alloc_info = MemoryArenaGetFrontAllocationLog(a_arena);

	if (ImGui::CollapsingHeader("Allocator Info"))
	{
		ImGui::Indent();
		ImGui::Text("Standard Reserved Memory: %zu", ARENA_DEFAULT_RESERVE);
		ImGui::Text("Commit size %zu", ARENA_DEFAULT_COMMIT);

		if (ImGui::CollapsingHeader("Main stack memory arena"))
		{
			ImGui::Indent();
			const size_t remaining = MemoryArenaSizeRemaining(a_arena);
			const size_t commited = MemoryArenaSizeCommited(a_arena);
			const size_t used = MemoryArenaSizeUsed(a_arena);

			const size_t real_size = commited;

			ImGui::Text("Memory Remaining: %zu", remaining);
			ImGui::Text("Memory Commited: %zu", commited);
			ImGui::Text("Memory Used: %zu", used);

			const float perc_calculation = 1.f / static_cast<float>(ARENA_DEFAULT_COMMIT);
			ImGui::Separator();
			ImGui::TextUnformatted("memory used till next commit");
			ImGui::ProgressBar(perc_calculation * static_cast<float>(RoundUp(used, ARENA_DEFAULT_COMMIT) - used));
		}
	}
}

using SceneObjectHandle = FrameworkHandle<struct SceneObjectHandleTag>;

constexpr size_t RENDER_OBJ_MAX = 128;
constexpr size_t RENDER_OBJ_CHILD_MAX = 16;

struct SceneObject
{
	const char* name;			//8
	MeshHandle mesh_handle;		//16
	uint32_t start_index;		//20
	uint32_t index_count;		//24
	MaterialHandle material;	//32

	TransformHandle transform;	//40

	SceneObjectHandle parent;	//48
	uint32_t child_count;		//52
	SceneObjectHandle childeren[RENDER_OBJ_CHILD_MAX];
};

struct SceneHierarchy
{
	TransformPool transform_pool;
	StaticSlotmap<SceneObject, SceneObjectHandle> scene_objects;

	size_t top_level_object_count;
	SceneObjectHandle top_level_objects[RENDER_OBJ_MAX];
};

static void ImGuiDisplaySceneObject(const SceneHierarchy& a_scene_hierarchy, const SceneObjectHandle& a_object)
{
	ImGui::Indent();

	const SceneObject& scene_object = a_scene_hierarchy.scene_objects.find(a_object);

	if (ImGui::CollapsingHeader(scene_object.name))
	{
		Transform& transform = a_scene_hierarchy.transform_pool.GetTransform(scene_object.transform);

		ImGui::InputFloat3("position", transform.m_pos.e);
		ImGui::InputFloat4("Rotation Quat (XYZW)", transform.m_rot.xyzw.e);
		ImGui::InputFloat3("scale", transform.m_scale.e);

		for (size_t i = 0; i < scene_object.child_count; i++)
		{
			ImGui::PushID(static_cast<int>(i));

			ImGuiDisplaySceneObject(a_scene_hierarchy, scene_object.childeren[i]);

			ImGui::PopID();
		}
	}


	ImGui::Unindent();
}

static void ImguiDisplaySceneHierarchy(const SceneHierarchy& a_scene_hierarchy)
{
	if (ImGui::CollapsingHeader("Scene Hierarchy"))
	{
		ImGui::Indent();

		for (size_t i = 0; i < a_scene_hierarchy.top_level_object_count; i++)
		{
			ImGui::PushID(static_cast<int>(i));

			ImGuiDisplaySceneObject(a_scene_hierarchy, a_scene_hierarchy.top_level_objects[i]);
			
			ImGui::PopID();
		}

		ImGui::Unindent();
	}
}

static SceneObjectHandle CreateSceneObjectViaModelNode(SceneHierarchy& a_scene_hierarchy, const Model& a_model, const Model::Node& a_node, const SceneObjectHandle a_parent)
{
	//decompose the matrix.
	float3 transform;
	float3 scale;
	Quat rotation;
	Float4x4DecomposeTransform(a_node.transform, transform, rotation, scale);

	const SceneObjectHandle scene_handle = a_scene_hierarchy.scene_objects.emplace(SceneObject());
	SceneObject& scene_obj = a_scene_hierarchy.scene_objects.find(scene_handle);
	scene_obj.name = a_node.name;
	scene_obj.mesh_handle = MeshHandle(BB_INVALID_HANDLE_64);
	scene_obj.start_index = 0;
	scene_obj.index_count = 0;
	scene_obj.material = MaterialHandle(BB_INVALID_HANDLE_64);
	scene_obj.transform = a_scene_hierarchy.transform_pool.CreateTransform(transform, rotation, scale);
	scene_obj.parent = a_parent;

	if (a_node.mesh_handle.IsValid())
	{
		for (uint32_t i = 0; i < a_node.primitive_count; i++)
		{
			BB_ASSERT(scene_obj.child_count < RENDER_OBJ_CHILD_MAX, "Too many childeren for a single scene object!");
			SceneObject prim_obj{};
			prim_obj.name = a_node.primitives[i].name;
			prim_obj.mesh_handle = a_node.mesh_handle;
			prim_obj.start_index = a_node.primitives[i].start_index;
			prim_obj.index_count = a_node.primitives[i].index_count;
			prim_obj.material = a_node.primitives[i].material;
			prim_obj.transform = a_scene_hierarchy.transform_pool.CreateTransform(float3(0, 0, 0));

			prim_obj.parent = scene_handle;
			scene_obj.childeren[scene_obj.child_count++] = a_scene_hierarchy.scene_objects.emplace(prim_obj);
		}
	}

	for (uint32_t i = 0; i < a_node.child_count; i++)
	{
		BB_ASSERT(scene_obj.child_count < RENDER_OBJ_CHILD_MAX, "Too many childeren for a single gameobject!");
		scene_obj.childeren[scene_obj.child_count++] = CreateSceneObjectViaModelNode(a_scene_hierarchy, a_model, a_node.childeren[i], a_parent);
	}

	return scene_handle;
}

static void CreateSceneObjectViaModel(SceneHierarchy& a_scene_hierarchy, const Model& a_model, const float3 a_position, const char* a_name)
{
	SceneObjectHandle top_level_handle = a_scene_hierarchy.scene_objects.emplace(SceneObject());
	SceneObject& top_level_object = a_scene_hierarchy.scene_objects.find(top_level_handle);
	top_level_object.name = a_name;
	top_level_object.mesh_handle = MeshHandle(BB_INVALID_HANDLE_64);
	top_level_object.start_index = 0;
	top_level_object.index_count = 0;
	top_level_object.material = MaterialHandle(BB_INVALID_HANDLE_64);
	top_level_object.parent = SceneObjectHandle(BB_INVALID_HANDLE_64);
	top_level_object.transform = a_scene_hierarchy.transform_pool.CreateTransform(a_position);

	top_level_object.child_count = a_model.root_node_count;
	BB_ASSERT(top_level_object.child_count < RENDER_OBJ_CHILD_MAX, "Too many childeren for a single scene object!");

	for (uint32_t i = 0; i < a_model.root_node_count; i++)
	{
		top_level_object.childeren[i] = CreateSceneObjectViaModelNode(a_scene_hierarchy, a_model, a_model.root_nodes[i], top_level_handle);
	}

	BB_ASSERT(a_scene_hierarchy.top_level_object_count < RENDER_OBJ_MAX, "Too many scene objects, increase the max");
	a_scene_hierarchy.top_level_objects[a_scene_hierarchy.top_level_object_count++] = top_level_handle;
}

static void DrawSceneObject(const SceneHierarchy& a_scene_hierarchy, const SceneObjectHandle a_scene_object, const float4x4& a_transform)
{
	const SceneObject& scene_object = a_scene_hierarchy.scene_objects.find(a_scene_object);
	
	const float4x4 local_transform = a_transform * a_scene_hierarchy.transform_pool.GetTransformMatrix(scene_object.transform);

	if (scene_object.mesh_handle.handle != BB_INVALID_HANDLE_64)
		DrawMesh(scene_object.mesh_handle, local_transform, scene_object.start_index, scene_object.index_count, scene_object.material);

	for (size_t i = 0; i < scene_object.child_count; i++)
	{
		DrawSceneObject(a_scene_hierarchy, scene_object.childeren[i], local_transform);
	}

}

static void DrawSceneHierarchy(const SceneHierarchy& a_scene_hierarchy)
{
	for (size_t i = 0; i < a_scene_hierarchy.top_level_object_count; i++)
	{
		// identity hack to awkwardly get the first matrix. 
		DrawSceneObject(a_scene_hierarchy, a_scene_hierarchy.top_level_objects[i], Float4x4Identity());
	}
}

int main(int argc, char** argv)
{
	(void)argc;

	BBInitInfo bb_init{};
	bb_init.exe_path = argv[0];
	bb_init.program_name = L"Modern Vulkan";
	InitBB(bb_init);

	SystemInfo sys_info;
	OSSystemInfo(sys_info);
	Threads::InitThreads(sys_info.processor_num);

	MemoryArena main_arena = MemoryArenaCreate();

	int window_width = 1280;
	int window_height = 720;
	const WindowHandle window = CreateOSWindow(
		BB::OS_WINDOW_STYLE::MAIN,
		250,
		200,
		window_width,
		window_height,
		L"Modern Vulkan");

	{
		const Asset::AssetManagerInitInfo asset_manager_info{};
		Asset::InitializeAssetManager(asset_manager_info);
	}

	RendererCreateInfo render_create_info;
	render_create_info.app_name = "modern vulkan";
	render_create_info.engine_name = "Building Block Engine";
	render_create_info.window_handle = window;
	render_create_info.swapchain_width = static_cast<uint32_t>(window_width);
	render_create_info.swapchain_height = static_cast<uint32_t>(window_height);
	render_create_info.debug = true;
	InitializeRenderer(main_arena, render_create_info);
	SetupImGuiInput(main_arena, window);
	Camera camera{ float3{2.0f, 2.0f, 2.0f}, 0.35f };

	SceneHierarchy scene_hierarchy;
	scene_hierarchy.scene_objects.Init(main_arena, RENDER_OBJ_MAX);
	scene_hierarchy.transform_pool.Init(main_arena, RENDER_OBJ_MAX);
	scene_hierarchy.top_level_object_count = 0;

	{
		const float4x4 projection = Float4x4Perspective(ToRadians(60.0f),
			static_cast<float>(render_create_info.swapchain_width) / static_cast<float>(render_create_info.swapchain_height),
			.001f, 10000.0f);
		BB::SetProjection(projection);
	}

	ShaderEffectHandle shader_effects[2];
	MemoryArenaScope(main_arena)
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

		BB_ASSERT(CreateShaderEffect(main_arena,
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
		async_assets[0].mesh_disk.name = "duck gltf";
		async_assets[0].mesh_disk.path = "../resources/models/Duck.gltf";
		async_assets[0].mesh_disk.shader_effects = Slice(shader_effects, _countof(shader_effects));

		async_assets[1].asset_type = Asset::ASYNC_ASSET_TYPE::MODEL;
		async_assets[1].load_type = Asset::ASYNC_LOAD_TYPE::MEMORY;
		async_assets[1].mesh_memory.name = "basic quad";
		async_assets[1].mesh_memory.vertices = Slice(vertices, _countof(vertices));
		async_assets[1].mesh_memory.indices = Slice(indices, _countof(indices));
		async_assets[1].mesh_memory.material = default_mat;
		Asset::LoadASync(Slice(async_assets, _countof(async_assets)));

		CreateSceneObjectViaModel(scene_hierarchy, *Asset::FindModelByPath(async_assets[0].mesh_disk.path), float3{ 0, 1, 1 }, "duck-y");
		CreateSceneObjectViaModel(scene_hierarchy, *Asset::FindModelByPath(async_assets[1].mesh_memory.name), float3{ 0, -1, 1 }, "quat-y");
	}

	LightHandle lights[2];

	{	//add some basic lights
		BB::FixedArray<CreateLightInfo, 2> light_create_info;
		light_create_info[0].color = float3(1, 1, 1);
		light_create_info[0].linear_distance = 0.35f;
		light_create_info[0].quadratic_distance = 0.44f;
		light_create_info[0].pos = float3(3, 0, 0);

		light_create_info[1].color = float3(1, 1, 1);
		light_create_info[1].linear_distance = 0.35f;
		light_create_info[1].quadratic_distance = 0.44f;
		light_create_info[1].pos = float3(0, 4, 0);

		CreateLights(light_create_info, lights);
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

		BB::SetView(camera.CalculateView());
		StartFrame();

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

		DebugWindowMemoryArena(main_arena);
		ImguiDisplaySceneHierarchy(scene_hierarchy);

		DrawSceneHierarchy(scene_hierarchy);

		EndFrame();

		delta_time = std::chrono::duration<float, std::chrono::seconds::period>(current_time - start_time).count();
		current_time = std::chrono::high_resolution_clock::now();
	}

	DestroyImGuiInput();
	DirectDestroyOSWindow(window);

	return 0;
}
