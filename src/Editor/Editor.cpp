#include "Editor.hpp"

#include "Program.h"
#include "BBThreadScheduler.hpp"
#include "HID.h"
#include "Math.inl"

#include "imgui.h"

#include "BBjson.hpp"

using namespace BB;

constexpr size_t EDITOR_VIEWPORT_ARRAY_SIZE = 16;
constexpr size_t EDITOR_MODEL_NAME_ARRAY_SIZE = 256;

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

static inline const char* ShaderStageToCChar(const SHADER_STAGE a_stage)
{
	switch (a_stage)
	{
	case SHADER_STAGE::VERTEX:			return "VERTEX";
	case SHADER_STAGE::FRAGMENT_PIXEL:	return "FRAGMENT_PIXEL";
	case SHADER_STAGE::NONE:
	case SHADER_STAGE::ALL:
	default:
		BB_ASSERT(false, "invalid shader stage for shader");
		return "error";
	}
}

static inline StackString<256> ShaderStagesToCChar(const SHADER_STAGE_FLAGS a_stage_flags)
{
	StackString<256> stages{};
	if ((static_cast<SHADER_STAGE_FLAGS>(SHADER_STAGE::VERTEX) & a_stage_flags) == a_stage_flags)
	{
		stages.append("VERTEX ");
	}
	if ((static_cast<SHADER_STAGE_FLAGS>(SHADER_STAGE::FRAGMENT_PIXEL) & a_stage_flags) == a_stage_flags)
	{
		stages.append("FRAGMENT_PIXEL ");
	}
	return stages;
}

static void DisplayGPUInfo(const GPUDeviceInfo& a_gpu_info)
{
	if (ImGui::CollapsingHeader("gpu info"))
	{
		ImGui::Indent();
		ImGui::TextUnformatted(a_gpu_info.name);
		if (ImGui::CollapsingHeader("memory heaps"))
		{
			ImGui::Indent();
			for (uint32_t i = 0; i < static_cast<uint32_t>(a_gpu_info.memory_heaps.size()); i++)
			{
				ImGui::Text("heap: %u", a_gpu_info.memory_heaps[i].heap_num);
				ImGui::Text("heap size: %lld", a_gpu_info.memory_heaps[i].heap_size);
				ImGui::Text("heap is device local %d", a_gpu_info.memory_heaps[i].heap_device_local);
			}
			ImGui::Unindent();
		}

		if (ImGui::CollapsingHeader("queue families"))
		{
			ImGui::Indent();
			for (size_t i = 0; i < a_gpu_info.queue_families.size(); i++)
			{
				ImGui::Text("family index: %u", a_gpu_info.queue_families[i].queue_family_index);
				ImGui::Text("queue count: %u", a_gpu_info.queue_families[i].queue_count);
				ImGui::Text("support compute: %d", a_gpu_info.queue_families[i].support_compute);
				ImGui::Text("support graphics: %d", a_gpu_info.queue_families[i].support_graphics);
				ImGui::Text("support transfer: %d", a_gpu_info.queue_families[i].support_transfer);
			}
			ImGui::Unindent();
		}
		ImGui::Unindent();
	}
}

void Editor::MainEditorImGuiInfo(const MemoryArena& a_arena)
{
	if (ImGui::Begin("general engine info"))
	{
		if (m_active_viewport == nullptr)
			ImGui::Text("current viewport: None");
		else
			ImGui::Text("current viewport: %s", m_active_viewport->viewport.GetName().c_str());

		ImGui::SliderFloat("camera speed", &m_cam_speed, m_cam_speed_min, m_cam_speed_max);

		DisplayGPUInfo(m_gpu_info);

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

void Editor::ThreadFuncForDrawing(MemoryArena&, void* a_param)
{
	ThreadFuncForDrawing_Params* param_in = reinterpret_cast<ThreadFuncForDrawing_Params*>(a_param);

	uint32_t back_buffer_index = param_in->back_buffer_index;
	Viewport& viewport = param_in->viewport;
	SceneHierarchy& scene_hierarchy = param_in->scene_hierarchy;
	RCommandList list = param_in->command_list;

	const RTexture render_target = viewport.StartRenderTarget(list, back_buffer_index);
	scene_hierarchy.DrawSceneHierarchy(list, render_target, viewport.GetExtent(), int2(), back_buffer_index);
	viewport.EndRenderTarget(list, render_target, IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL);
}

void Editor::Init(MemoryArena& a_arena, const WindowHandle a_window, const uint2 a_window_extent, const size_t a_editor_memory)
{
	m_main_window = a_window;
	m_app_window_extent = a_window_extent;

	SetupImGuiInput(a_arena, m_main_window);

	m_gpu_info = GetGPUInfo(a_arena);

	m_editor_allocator.Initialize(a_arena, a_editor_memory);
	void* viewports_mem = m_editor_allocator.Alloc(EDITOR_VIEWPORT_ARRAY_SIZE * sizeof(decltype(m_viewport_and_scenes)::TYPE), 16);
	void* loaded_model_names = m_editor_allocator.Alloc(EDITOR_MODEL_NAME_ARRAY_SIZE * sizeof(decltype(m_loaded_models_names)::TYPE), 16);

	m_viewport_and_scenes.Init(viewports_mem, EDITOR_VIEWPORT_ARRAY_SIZE);
	m_loaded_models_names.Init(loaded_model_names, EDITOR_MODEL_NAME_ARRAY_SIZE);

	MaterialSystemCreateInfo material_system_init;
	material_system_init.max_materials = 128;
	material_system_init.max_shader_effects = 64;
	material_system_init.default_2d_vertex.path = "../../resources/shaders/hlsl/Imgui.hlsl";
	material_system_init.default_2d_vertex.entry = "VertexMain";
	material_system_init.default_2d_vertex.stage = SHADER_STAGE::VERTEX;
	material_system_init.default_2d_vertex.next_stages = static_cast<SHADER_STAGE_FLAGS>(SHADER_STAGE::FRAGMENT_PIXEL);

	material_system_init.default_2d_fragment.path = "../../resources/shaders/hlsl/Imgui.hlsl";
	material_system_init.default_2d_fragment.entry = "FragmentMain";
	material_system_init.default_2d_fragment.stage = SHADER_STAGE::FRAGMENT_PIXEL;
	material_system_init.default_2d_fragment.next_stages = static_cast<SHADER_STAGE_FLAGS>(SHADER_STAGE::NONE);

	material_system_init.default_3d_vertex.path = "../../resources/shaders/hlsl/Debug.hlsl";
	material_system_init.default_3d_vertex.entry = "VertexMain";
	material_system_init.default_3d_vertex.stage = SHADER_STAGE::VERTEX;
	material_system_init.default_3d_vertex.next_stages = static_cast<SHADER_STAGE_FLAGS>(SHADER_STAGE::FRAGMENT_PIXEL);

	material_system_init.default_3d_fragment.path = "../../resources/shaders/hlsl/Debug.hlsl";
	material_system_init.default_3d_fragment.entry = "FragmentMain";
	material_system_init.default_3d_fragment.stage = SHADER_STAGE::FRAGMENT_PIXEL;
	material_system_init.default_3d_fragment.next_stages = static_cast<SHADER_STAGE_FLAGS>(SHADER_STAGE::NONE);

	Material::InitMaterialSystem(a_arena, material_system_init);

	m_imgui_material = Material::GetDefaultMaterial(PASS_TYPE::GLOBAL, MATERIAL_TYPE::MATERIAL_2D);
}

void Editor::Destroy()
{
	DestroyImGuiInput();
	DirectDestroyOSWindow(m_main_window);
}

void Editor::CreateSceneHierarchyViaJson(MemoryArena& a_arena, SceneHierarchy& a_hierarchy, const uint32_t a_back_buffer_count, const char* a_json_path)
{
	JsonParser json_file(a_json_path);
	CreateSceneHierarchyViaJson(a_arena, a_hierarchy, a_back_buffer_count, json_file);
}

void Editor::CreateSceneHierarchyViaJson(MemoryArena& a_arena, SceneHierarchy& a_hierarchy, const uint32_t a_back_buffer_count, const JsonParser& a_parsed_file)
{
	const JsonObject& scene_obj = a_parsed_file.GetRootNode()->GetObject().Find("scene")->GetObject();
	{
		a_hierarchy.Init(a_arena, a_back_buffer_count, Asset::FindOrCreateString(scene_obj.Find("name")->GetString()));
	}

	const JsonList& scene_objects = scene_obj.Find("scene_objects")->GetList();

	for (size_t i = 0; i < scene_objects.node_count; i++)
	{
		const JsonObject& sce_obj = scene_objects.nodes[i]->GetObject();
		const char* model_name = scene_objects.nodes[i]->GetObject().Find("file_name")->GetString();
		const char* obj_name = scene_objects.nodes[i]->GetObject().Find("file_name")->GetString();
		const Model* model = Asset::FindModelByName(model_name);
		BB_ASSERT(model != nullptr, "model failed to be found");
		const JsonList& position_list = sce_obj.Find("position")->GetList();
		BB_ASSERT(position_list.node_count == 3, "scene_object position in scene json is not 3 elements");
		float3 position;
		position.x = position_list.nodes[0]->GetNumber();
		position.y = position_list.nodes[1]->GetNumber();
		position.z = position_list.nodes[2]->GetNumber();

		a_hierarchy.CreateSceneObjectViaModel(*model, position, obj_name);
	}

	const JsonList& lights = scene_obj.Find("lights")->GetList();
	for (size_t i = 0; i < lights.node_count; i++)
	{
		const JsonObject& light_obj = lights.nodes[i]->GetObject();
		LightCreateInfo light_info;

		const char* light_type = light_obj.Find("light_type")->GetString();
		if (strcmp(light_type, "spotlight") == 0)
			light_info.light_type = LIGHT_TYPE::SPOT_LIGHT;
		else if (strcmp(light_type, "pointlight") == 0)
			light_info.light_type = LIGHT_TYPE::POINT_LIGHT;
		else
			BB_ASSERT(false, "invalid light type in json");

		const JsonList& position = light_obj.Find("position")->GetList();
		BB_ASSERT(position.node_count == 3, "light position in scene json is not 3 elements");
		light_info.pos.x = position.nodes[0]->GetNumber();
		light_info.pos.y = position.nodes[1]->GetNumber();
		light_info.pos.z = position.nodes[2]->GetNumber();

		const JsonList& color = light_obj.Find("color")->GetList();
		BB_ASSERT(color.node_count == 3, "light color in scene json is not 3 elements");
		light_info.color.x = color.nodes[0]->GetNumber();
		light_info.color.y = color.nodes[1]->GetNumber();
		light_info.color.z = color.nodes[2]->GetNumber();

		light_info.specular_strength = light_obj.Find("specular_strength")->GetNumber();
		light_info.radius_constant = light_obj.Find("constant")->GetNumber();
		light_info.radius_linear = light_obj.Find("linear")->GetNumber();
		light_info.radius_quadratic = light_obj.Find("quadratic")->GetNumber();

		if (light_info.light_type == LIGHT_TYPE::SPOT_LIGHT)
		{
			const JsonList& spot_dir = light_obj.Find("spotlight_dir")->GetList();
			BB_ASSERT(color.node_count == 3, "light spotlight_dir in scene json is not 3 elements");
			light_info.spotlight_direction.x = spot_dir.nodes[0]->GetNumber();
			light_info.spotlight_direction.y = spot_dir.nodes[1]->GetNumber();
			light_info.spotlight_direction.z = spot_dir.nodes[2]->GetNumber();

			light_info.cutoff_radius = light_obj.Find("cutoff_radius")->GetNumber();
		}

		const StringView light_name = Asset::FindOrCreateString(light_obj.Find("name")->GetString());
		a_hierarchy.CreateSceneObjectAsLight(light_info, light_name.c_str());
	}
}

void Editor::RegisterSceneHierarchy(MemoryArena& a_arena, SceneHierarchy& a_hierarchy, const uint2 a_window_extent, const uint32_t a_back_buffer_count)
{
	ViewportAndScene viewport_scene{ a_hierarchy };
	StackString<256> viewport_name{ a_hierarchy.m_scene_name.c_str(),  a_hierarchy.m_scene_name.size() };
	viewport_name.append(" viewport");
	viewport_scene.viewport.Init(a_arena, a_window_extent, int2(), a_back_buffer_count, Asset::FindOrCreateString(viewport_name.GetView()));

	m_viewport_and_scenes.push_back(viewport_scene);
}

void Editor::Update(MemoryArena& a_arena, const float a_delta_time, const Slice<InputEvent> a_input_events)
{
	for (size_t i = 0; i < a_input_events.size(); i++)
	{
		const InputEvent& ip = a_input_events[i];
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
				m_active_viewport->camera.Move(cam_move * m_cam_speed);
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

			if (mi.wheel_move)
			{
				m_cam_speed = Clampf(m_cam_speed + static_cast<float>(mi.wheel_move) * 0.1f,
					m_cam_speed_min,
					m_cam_speed_max);
			}

			for (size_t view_i = 0; view_i < m_viewport_and_scenes.size(); view_i++)
			{
				ViewportAndScene& vs = m_viewport_and_scenes[view_i];
				if (vs.viewport.PositionWithinViewport(uint2(static_cast<unsigned int>(mi.mouse_pos.x), static_cast<unsigned int>(mi.mouse_pos.y))))
				{
					m_active_viewport = &vs;
					break;
				}
			}

			if (!m_freeze_cam && m_active_viewport)
			{
				m_active_viewport->camera.Rotate(mouse_move.x, mouse_move.y);
			}
		}
	}
	MemoryArenaScope(a_arena)
	{
		//HACKY, but for now ok.
		const uint32_t command_list_count = Max(m_viewport_and_scenes.size(), 1u);
		CommandPool* pools = ArenaAllocArr(a_arena, CommandPool, command_list_count);
		RCommandList* lists = ArenaAllocArr(a_arena, RCommandList, command_list_count);
		pools[0] = GetGraphicsCommandPool();
		lists[0] = pools[0].StartCommandList();
		for (uint32_t i = 1; i < command_list_count; i++)
		{
			pools[i] = GetGraphicsCommandPool();
			lists[i] = pools[i].StartCommandList();
		}

		StartFrameInfo start_info;
		start_info.delta_time = a_delta_time;
		start_info.mouse_pos = m_previous_mouse_pos;

		uint32_t back_buffer_index;
		StartFrame(lists[0], start_info, back_buffer_index);

		if (ImGui::Begin("Editor - Renderer"))
		{
			ImGuiDisplayShaderEffects();
			ImGuiDisplayMaterials();
		}
		ImGui::End();



		Asset::ShowAssetMenu(a_arena);
		MainEditorImGuiInfo(a_arena);

		ThreadTask* thread_tasks = ArenaAllocArr(a_arena, ThreadTask, m_viewport_and_scenes.size());

		for (size_t i = 0; i < m_viewport_and_scenes.size(); i++)
		{
			ViewportAndScene& vs = m_viewport_and_scenes[i];
			vs.scene.SetView(vs.camera.CalculateView());

			ImguiDisplaySceneHierarchy(vs.scene);

			bool resized = false;
			vs.viewport.DrawImgui(resized, back_buffer_index);
			if (resized)
			{
				vs.scene.SetProjection(vs.viewport.CreateProjection(60.f, 0.001f, 10000.0f));
			}
			ThreadFuncForDrawing_Params* draw_params = ArenaAllocType(a_arena, ThreadFuncForDrawing_Params) {
				back_buffer_index,
				vs.viewport,
				vs.scene,
				lists[i]
			};

			thread_tasks[i] = Threads::StartTaskThread(ThreadFuncForDrawing, draw_params, L"scene draw task");
		}

		for (size_t i = 0; i < m_viewport_and_scenes.size(); i++)
		{
			Threads::WaitForTask(thread_tasks[i]);
		}

		// TODO, async this
		for (size_t i = 1; i < command_list_count; i++)
		{
			pools[i].EndCommandList(lists[i]);
		}


		Slice imgui_shaders = Material::GetMaterialShaders(m_imgui_material);
		EndFrame(lists[0], imgui_shaders[0], imgui_shaders[1], back_buffer_index);

		pools[0].EndCommandList(lists[0]);

		const uint32_t scene_fence_count = m_viewport_and_scenes.size();
		RFence* scene_fences = ArenaAllocArr(a_arena, RFence, scene_fence_count);
		uint64_t* scene_fence_values = ArenaAllocArr(a_arena, uint64_t, scene_fence_count);

		for (uint32_t i = 0; i < scene_fence_count; i++)
		{
			m_viewport_and_scenes[i].scene.IncrementNextFenceValue(&scene_fences[i], &scene_fence_values[i]);
		}
		
		uint64_t present_queue_value;
		PresentFrame(Slice(pools, command_list_count), scene_fences, scene_fence_values, scene_fence_count, present_queue_value);
	}
}

ThreadTask Editor::LoadAssets(const Slice<Asset::AsyncAsset> a_asyn_assets, Editor* a_editor)
{
	// maybe have each thread have it's own memory arena
	MemoryArena load_arena = MemoryArenaCreate();

	LoadAssetsAsync_params* params = ArenaAllocType(load_arena, LoadAssetsAsync_params);
	params->assets = ArenaAllocArr(load_arena, Asset::AsyncAsset, a_asyn_assets.size());
	memcpy(params->assets, a_asyn_assets.data(), a_asyn_assets.sizeInBytes());
	params->asset_count = a_asyn_assets.size();
	params->arena = load_arena;
	params->editor = a_editor;

	return Threads::StartTaskThread(Editor::LoadAssetsAsync, params);
}

void Editor::ImguiDisplaySceneHierarchy(SceneHierarchy& a_hierarchy)
{
	if (ImGui::Begin(a_hierarchy.m_scene_name.c_str()))
	{
		ImGui::Indent();

		ImguiCreateSceneObject(a_hierarchy);


		for (size_t i = 0; i < a_hierarchy.m_top_level_object_count; i++)
		{
			ImGui::PushID(static_cast<int>(i));

			ImGuiDisplaySceneObject(a_hierarchy, a_hierarchy.m_top_level_objects[i]);

			ImGui::PopID();
		}

		ImGui::Unindent();
	}
	ImGui::End();
}

void Editor::ImGuiDisplaySceneObject(SceneHierarchy& a_hierarchy, const SceneObjectHandle a_object)
{
	SceneObject& scene_object = a_hierarchy.m_scene_objects.find(a_object);
	ImGui::PushID(static_cast<int>(a_object.handle));

	if (ImGui::CollapsingHeader(scene_object.name))
	{
		ImGui::Indent();
		Transform& transform = a_hierarchy.m_transform_pool.GetTransform(scene_object.transform);

		bool position_changed = false;

		if (ImGui::TreeNodeEx("transform"))
		{
			if (ImGui::InputFloat3("position", transform.m_pos.e))
			{
				position_changed = true;
			}

			ImGui::InputFloat4("rotation quat (xyzw)", transform.m_rot.xyzw.e);
			ImGui::InputFloat3("scale", transform.m_scale.e);
			ImGui::TreePop();
		}

		if (scene_object.mesh_info.index_count > 0)
		{
			if (ImGui::TreeNodeEx("rendering"))
			{
				ImGui::Indent();

				if (ImGui::TreeNodeEx("material"))
				{
					ImGui::Indent();
					Material::GetMaterialInstance(scene_object.mesh_info.material);

					if (ImGui::TreeNodeEx("switch material"))
					{
						ImGui::Indent();

						Slice materials = Material::GetAllMaterials();
						for (size_t i = 0; i < materials.size(); i++)
						{
							const MaterialInstance& new_mat = materials[i];
							if (ImGui::Button(new_mat.name.c_str()))
							{
								scene_object.mesh_info.material = new_mat.handle;
							}
						}

						ImGui::TreePop();
						ImGui::Unindent();
					}

					ImGui::TreePop();
					ImGui::Unindent();
				}

				ImGui::Unindent();
				ImGui::TreePop();
			}
		}

		if (scene_object.light_handle.IsValid())
		{
			if (ImGui::TreeNodeEx("light object"))
			{
				ImGui::Indent();
				Light& light = a_hierarchy.GetLight(scene_object.light_handle);

				if (position_changed)
				{
					light.pos = transform.m_pos;
				}

				ImGui::InputFloat3("color", light.color.e);
				ImGui::InputFloat("linear radius", &light.radius_linear);
				ImGui::InputFloat("quadratic radius", &light.radius_quadratic);

				if (ImGui::Button("remove light"))
				{
					a_hierarchy.FreeLight(scene_object.light_handle);
					scene_object.light_handle = LightHandle(BB_INVALID_HANDLE_64);
				}
				ImGui::Unindent();
				ImGui::TreePop();
			}
		}
		else
		{
			if (ImGui::Button("create light"))
			{
				BB_UNIMPLEMENTED();
				//CreateLightInfo light_create_info;
				//light_create_info.pos = transform.m_pos;
				//light_create_info.color = float3(1.f, 1.f, 1.f);
				//light_create_info.linear_distance = 0.35f;
				//light_create_info.quadratic_distance = 0.44f;
				//scene_object.light_handle = CreateLight(a_hierarchy.m_render_scene, light_create_info);
			}
		}

		for (size_t i = 0; i < scene_object.child_count; i++)
		{
			ImGuiDisplaySceneObject(a_hierarchy, scene_object.children[i]);
		}

		ImguiCreateSceneObject(a_hierarchy, a_object);

		ImGui::Unindent();
	}

	ImGui::PopID();
}

void Editor::ImguiCreateSceneObject(SceneHierarchy& a_hierarchy, const SceneObjectHandle a_parent)
{
	if (ImGui::TreeNodeEx("create scene object menu"))
	{
		ImGui::Indent();
		static StringView mesh_name{};

		if (ImGui::TreeNodeEx("mesh_list"))
		{
			ImGui::Indent();

			if (ImGui::Button("set empty"))
			{
				mesh_name = StringView();
			}
			for (size_t i = 0; i < m_loaded_models_names.size(); i++)
			{
				if (ImGui::Button(m_loaded_models_names[i].c_str()))
				{
					mesh_name = m_loaded_models_names[i];
				}
			}

			ImGui::Unindent();
			ImGui::TreePop();
		}

		if (ImGui::Button("create scene object"))
		{
			BB_ASSERT(a_hierarchy.m_top_level_object_count <= a_hierarchy.m_scene_objects.capacity(), "Too many render object childeren for this object!");
			if (mesh_name.size() != 0)
			{
				const Model* model = Asset::FindModelByName(mesh_name.c_str());
				BB_ASSERT(model, "invalid model, this is not possible as we remember loaded models in the editor. has it been deleted?");
				a_hierarchy.CreateSceneObjectViaModel(*model, float3(0.f, 0.f, 0.f), mesh_name.c_str(), a_parent);
			}
			else
				a_hierarchy.CreateSceneObject(float3(0.f, 0.f, 0.f), "default", a_parent);
		}

		ImGui::Unindent();
		ImGui::TreePop();
	}
}

void Editor::ImGuiDisplayShaderEffect(const CachedShaderInfo& a_shader_info) const
{
	if (ImGui::CollapsingHeader(a_shader_info.create_info.name))
	{
		ImGui::Indent();
		ImGui::Text("HANDLE: %u | GENERATION: %u", a_shader_info.handle.index, a_shader_info.handle.extra_index);
		ImGui::Text("ENTRY POINT: %s", a_shader_info.create_info.shader_entry);
		ImGui::Text("SHADER STAGE: %s", ShaderStageToCChar(a_shader_info.create_info.stage));
		const StackString<256> next_stages = ShaderStagesToCChar(a_shader_info.create_info.next_stages);
		ImGui::Text("NEXT EXPECTED STAGES: %s", next_stages.c_str());

		if (ImGui::Button("Reload Shader"))
		{
			BB_UNIMPLEMENTED();
			//BB_ASSERT(ReloadShaderEffect(effect.handle, effect.shader_data), "something went wrong with reloading a shader");
		}
		ImGui::Unindent();
	}
}

void Editor::ImGuiDisplayShaderEffects()
{
	if (ImGui::CollapsingHeader("shader effects"))
	{
		ImGui::Indent();
		const Slice shaders = Material::GetAllCachedShaders();
		for (size_t i = 0; i < shaders.size(); i++)
		{
			ImGui::PushID(static_cast<int>(i));
			// rework this?
			ImGuiDisplayShaderEffect(shaders[i]);
			ImGui::PopID();
		}
		ImGui::Unindent();
	}
}

void Editor::ImGuiDisplayMaterial(const MaterialInstance& a_material) const
{
	if (ImGui::CollapsingHeader(a_material.name.c_str()))
	{
		ImGui::PushID(static_cast<int>(a_material.handle.index));
		ImGui::Indent();

		for (size_t eff_index = 0; eff_index < a_material.shader_effect_count; eff_index++)
		{
			ImGui::PushID(static_cast<int>(eff_index));
			//ImGuiDisplayShaderEffect();
			ImGui::PopID();
		}

		ImGui::PopID();
		ImGui::Unindent();
	}
}

void Editor::ImGuiDisplayMaterials()
{
	if (ImGui::CollapsingHeader("materials"))
	{
		ImGui::Indent();
		const Slice materials = Material::GetAllMaterials();
		for (uint32_t i = 0; i < materials.size(); i++)
		{
			ImGuiDisplayMaterial(materials[i]);
		}
		ImGui::Unindent();
	}
}

void Editor::LoadAssetsAsync(MemoryArena&, void* a_params)
{
	LoadAssetsAsync_params* params = reinterpret_cast<LoadAssetsAsync_params*>(a_params);
	MemoryArena load_arena = MemoryArenaCreate();
	//Editor& editor = *params->editor;

	Slice loaded_assets = Asset::LoadAssets(load_arena, Slice(params->assets, params->asset_count));

	for (size_t i = 0; i < loaded_assets.size(); i++)
	{
		if (loaded_assets[i].type == Asset::ASYNC_ASSET_TYPE::MODEL)
			params->editor->m_loaded_models_names.push_back(loaded_assets[i].name);
	}

	MemoryArenaFree(load_arena);
}
