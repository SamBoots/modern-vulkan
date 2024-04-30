#include "Editor.hpp"

#include "Program.h"
#include "BBThreadScheduler.hpp"
#include "HID.h"
#include "Math.inl"

#include "imgui.h"

#include "BBjson.hpp"

using namespace BB;

constexpr size_t EDITOR_MATERIAL_ARRAY_SIZE = 4092;
constexpr size_t EDITOR_SHADER_EFFECTS_ARRAY_SIZE = 1024;
constexpr size_t EDITOR_VIEWPORT_ARRAY_SIZE = 16;

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
			ImGui::Text("current viewport: %s", m_active_viewport->viewport.GetName());

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

	Viewport& viewport = param_in->viewport;
	SceneHierarchy& scene_hierarchy = param_in->scene_hierarchy;
	RCommandList list = param_in->command_list;

	viewport.DrawScene(list, scene_hierarchy);
}

void Editor::Init(MemoryArena& a_arena, const WindowHandle a_window, const uint2 a_window_extent, const size_t a_editor_memory)
{
	m_main_window = a_window; 
	m_app_window_extent = a_window_extent;

	SetupImGuiInput(a_arena, m_main_window);

	m_gpu_info = GetGPUInfo(a_arena);

	m_editor_allocator.Initialize(a_arena, a_editor_memory);
	void* materials_mem = m_editor_allocator.Alloc(EDITOR_MATERIAL_ARRAY_SIZE * sizeof(decltype(m_materials)::TYPE), 16);
	void* shader_effects_mem = m_editor_allocator.Alloc(EDITOR_SHADER_EFFECTS_ARRAY_SIZE * sizeof(decltype(m_shader_effects)::TYPE), 16);
	void* viewports_mem = m_editor_allocator.Alloc(EDITOR_VIEWPORT_ARRAY_SIZE * sizeof(decltype(m_viewport_and_scenes)::TYPE), 16);
	m_materials.Init(materials_mem, EDITOR_MATERIAL_ARRAY_SIZE);
	m_shader_effects.Init(shader_effects_mem, EDITOR_SHADER_EFFECTS_ARRAY_SIZE);
	m_viewport_and_scenes.Init(viewports_mem, EDITOR_VIEWPORT_ARRAY_SIZE);

	MemoryArenaScope(a_arena)
	{
		ShaderEffectHandle shader_effs[2];
		Buffer imgui_shader = ReadOSFile(a_arena, "../../resources/shaders/hlsl/Imgui.hlsl");

		CreateShaderEffectInfo shaders[2];
		shaders[0].name = "imgui vertex shader";
		shaders[0].shader_data = imgui_shader;
		shaders[0].shader_entry = "VertexMain";
		shaders[0].stage = SHADER_STAGE::VERTEX;
		shaders[0].next_stages = static_cast<SHADER_STAGE_FLAGS>(SHADER_STAGE::FRAGMENT_PIXEL);
		shaders[0].push_constant_space = sizeof(ShaderIndices2D);
		shaders[0].pass_type = RENDER_PASS_TYPE::STANDARD_3D;

		shaders[1].name = "imgui Fragment shader";
		shaders[1].shader_data = imgui_shader;
		shaders[1].shader_entry = "FragmentMain";
		shaders[1].stage = SHADER_STAGE::FRAGMENT_PIXEL;
		shaders[1].next_stages = static_cast<SHADER_STAGE_FLAGS>(SHADER_STAGE::NONE);
		shaders[1].push_constant_space = sizeof(ShaderIndices2D);
		shaders[1].pass_type = RENDER_PASS_TYPE::STANDARD_3D;

		BB_ASSERT(this->CreateShaderEffect(a_arena, Slice(shaders, _countof(shaders)), shader_effs),
			"Failed to create imgui shaders");

		m_imgui_vertex = shader_effs[0];
		m_imgui_fragment = shader_effs[1];
	}
}

void Editor::CreateViewportViaJson(MemoryArena& a_arena, const char* a_json_path, const char* a_viewport_name, const uint2 a_window_extent, const float3 a_clear_color)
{
	ViewportAndScene viewport_scene;

	viewport_scene.viewport.Init(a_arena, a_window_extent, uint2(), a_viewport_name);

	JsonParser viewport_json(a_json_path);
	viewport_json.Parse();
	auto viewer_list = SceneHierarchy::PreloadAssetsFromJson(a_arena, viewport_json);
	const ThreadTask view_upload = LoadAssets(Slice(viewer_list.data(), viewer_list.size()), a_viewport_name);

	Threads::WaitForTask(view_upload);

	viewport_scene.scene.InitViaJson(a_arena, viewport_json);
	viewport_scene.scene.SetClearColor(a_clear_color);
	m_viewport_and_scenes.push_back(viewport_scene);
}

void Editor::Destroy()
{
	DestroyImGuiInput();
	DirectDestroyOSWindow(m_main_window);
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

	CommandPool graphics_command_pools[2]{ GetGraphicsCommandPool(), GetGraphicsCommandPool() };
	const RCommandList main_list = graphics_command_pools[0].StartCommandList();

	StartFrameInfo start_info;
	start_info.delta_time = a_delta_time;
	start_info.mouse_pos = m_previous_mouse_pos;

	StartFrame(main_list, start_info);

	if (ImGui::Begin("Editor - Renderer"))
	{
		ImGuiDisplayShaderEffects();
		ImGuiDisplayMaterials();
		ImGuiCreateMaterial();
	}
	ImGui::End();


	MemoryArenaScope(a_arena)
	{
		Asset::ShowAssetMenu(a_arena);
		MainEditorImGuiInfo(a_arena);

		ThreadTask* thread_tasks = ArenaAllocArr(a_arena, ThreadTask, m_viewport_and_scenes.size());

		//HACKY, but for now ok.
		const RCommandList lists[2]{ main_list, graphics_command_pools[1].StartCommandList() };

		for (size_t i = 0; i < m_viewport_and_scenes.size(); i++)
		{
			ViewportAndScene& vs = m_viewport_and_scenes[i];
			vs.scene.SetView(vs.camera.CalculateView());

			ImguiDisplaySceneHierarchy(vs.scene);

			bool resized = false;
			vs.viewport.DrawImgui(resized);
			if (resized)
			{
				vs.scene.SetProjection(vs.viewport.CreateProjection(60.f, 0.001f, 10000.0f));
			}
			ThreadFuncForDrawing_Params* draw_params = ArenaAllocType(a_arena, ThreadFuncForDrawing_Params) {
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
		graphics_command_pools[1].EndCommandList(lists[1]);

		EndFrame(main_list, m_imgui_vertex, m_imgui_fragment);

		graphics_command_pools[0].EndCommandList(main_list);
		uint64_t fence_value;
		PresentFrame(Slice(graphics_command_pools, _countof(graphics_command_pools)), fence_value);
	}
}

bool Editor::CreateShaderEffect(MemoryArena& a_temp_arena, const Slice<CreateShaderEffectInfo> a_create_infos, ShaderEffectHandle* const a_handles)
{
	bool ret_val = ::CreateShaderEffect(a_temp_arena, a_create_infos, a_handles);

	for (size_t i = 0; i < a_create_infos.size(); i++)
	{
		const CreateShaderEffectInfo& ci = a_create_infos[i];

		ShaderEffectInfo shader_eff_info;
		shader_eff_info.handle = a_handles[i];
		// god I hate this
		new (&shader_eff_info.name) StringView(Asset::FindOrCreateString(ci.name));
		new (&shader_eff_info.entry_point) StringView(Asset::FindOrCreateString(ci.shader_entry));
		shader_eff_info.shader_stage = ci.stage;
		shader_eff_info.next_stages = ci.next_stages;
		shader_eff_info.shader_data.size = ci.shader_data.size;
		shader_eff_info.shader_data.data = m_editor_allocator.Alloc(shader_eff_info.shader_data.size, 4);
		memcpy(shader_eff_info.shader_data.data, ci.shader_data.data, shader_eff_info.shader_data.size);
		m_shader_effects.push_back(shader_eff_info);
		
	}

	return ret_val;
}

const MaterialHandle Editor::CreateMaterial(const CreateMaterialInfo& a_create_info)
{
	const MaterialHandle material = ::CreateMaterial(a_create_info);

	MaterialInfo mat_info;
	mat_info.handle = material;
	new (&mat_info.name) StringView(Asset::FindOrCreateString(a_create_info.name));
	mat_info.shader_handle_count = a_create_info.shader_effects.size();
	mat_info.shader_handles = reinterpret_cast<ShaderEffectHandle*>(m_editor_allocator.Alloc(mat_info.shader_handle_count * sizeof(ShaderEffectHandle), alignof(ShaderEffectHandle)));
	memcpy(mat_info.shader_handles, a_create_info.shader_effects.data(), a_create_info.shader_effects.sizeInBytes());

	m_materials.push_back(mat_info);
	return material;
}

ThreadTask Editor::LoadAssets(const Slice<Asset::AsyncAsset> a_asyn_assets, const char* a_cmd_list_name)
{
	// maybe have each thread have it's own memory arena
	MemoryArena load_arena = MemoryArenaCreate();

	LoadAssetsAsync_params* params = ArenaAllocType(load_arena, LoadAssetsAsync_params);
	params->assets = ArenaAllocArr(load_arena, Asset::AsyncAsset, a_asyn_assets.size());
	memcpy(params->assets, a_asyn_assets.data(), a_asyn_assets.sizeInBytes());
	params->asset_count = a_asyn_assets.size();
	params->cmd_list_name = a_cmd_list_name;
	params->arena = load_arena;

	return Threads::StartTaskThread(Editor::LoadAssetsAsync, params);
}

void Editor::ImguiDisplaySceneHierarchy(SceneHierarchy& a_hierarchy)
{
	if (ImGui::Begin(a_hierarchy.m_scene_name.c_str()))
	{
		ImGui::Indent();
		if (ImGui::Button("create scene object"))
		{
			BB_ASSERT(a_hierarchy.m_top_level_object_count <= a_hierarchy.m_scene_objects.capacity(), "Too many render object childeren for this object!");
			a_hierarchy.m_top_level_objects[a_hierarchy.m_top_level_object_count++] = a_hierarchy.CreateSceneObjectEmpty(SceneObjectHandle(BB_INVALID_HANDLE_64));
		}

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

		if (scene_object.mesh_handle.IsValid())
		{
			if (ImGui::TreeNodeEx("rendering"))
			{
				ImGui::Indent();

				if (ImGui::TreeNodeEx("material"))
				{
					ImGui::Indent();
					ImGuiDisplayMaterial(scene_object.material);

					if (ImGui::TreeNodeEx("switch material"))
					{
						ImGui::Indent();

						for (size_t i = 0; i < m_materials.size(); i++)
						{
							const MaterialInfo new_mat = m_materials[i];
							if (ImGui::Button(new_mat.name.c_str()))
							{
								scene_object.material = new_mat.handle;
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
				PointLight& light = GetLight(a_hierarchy.m_render_scene, scene_object.light_handle);

				if (position_changed)
				{
					light.pos = transform.m_pos;
				}

				ImGui::InputFloat3("color", light.color.e);
				ImGui::InputFloat("linear radius", &light.radius_linear);
				ImGui::InputFloat("quadratic radius", &light.radius_quadratic);

				if (ImGui::Button("remove light"))
				{
					FreeLight(a_hierarchy.m_render_scene, scene_object.light_handle);
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
				CreateLightInfo light_create_info;
				light_create_info.pos = transform.m_pos;
				light_create_info.color = float3(1.f, 1.f, 1.f);
				light_create_info.linear_distance = 0.35f;
				light_create_info.quadratic_distance = 0.44f;
				scene_object.light_handle = CreateLight(a_hierarchy.m_render_scene, light_create_info);
			}
		}

		for (size_t i = 0; i < scene_object.child_count; i++)
		{
			ImGuiDisplaySceneObject(a_hierarchy, scene_object.childeren[i]);
		}

		if (ImGui::Button("create scene object"))
		{
			BB_ASSERT(scene_object.child_count <= SCENE_OBJ_CHILD_MAX, "Too many render object childeren for this object!");
			scene_object.childeren[scene_object.child_count++] = a_hierarchy.CreateSceneObjectEmpty(a_object);
		}

		ImGui::Unindent();
	}

	ImGui::PopID();
}

void Editor::ImGuiDisplayShaderEffect(const ShaderEffectHandle a_handle) const
{
	const Editor::ShaderEffectInfo& effect = m_shader_effects[a_handle.handle];
	if (ImGui::CollapsingHeader(effect.name.c_str()))
	{
		ImGui::Indent();

		ImGui::Text("ENTRY POINT: %s", effect.entry_point.c_str());
		ImGui::Text("SHADER STAGE: %s", ShaderStageToCChar(effect.shader_stage));
		const StackString<256> next_stages = ShaderStagesToCChar(effect.next_stages);
		ImGui::Text("NEXT EXPECTED STAGES: %s", next_stages.c_str());

		if (ImGui::Button("Reload Shader"))
		{
			BB_ASSERT(ReloadShaderEffect(effect.handle, effect.shader_data), "something went wrong with reloading a shader");
		}
		if (ImGui::CollapsingHeader("shader code"))
		{
			ImGui::TextUnformatted(reinterpret_cast<const char*>(effect.shader_data.data));
		}
		ImGui::Unindent();
	}
}

void Editor::ImGuiDisplayShaderEffects()
{
	if (ImGui::CollapsingHeader("shader effects"))
	{
		ImGui::Indent();
		for (size_t i = 0; i < m_shader_effects.size(); i++)
		{
			ImGui::PushID(static_cast<int>(i));
			// rework this?
			ImGuiDisplayShaderEffect(ShaderEffectHandle(i));
			ImGui::PopID();
		}
		ImGui::Unindent();
	}
}

void Editor::ImGuiDisplayMaterial(const MaterialHandle a_handle) const
{
	const MaterialInfo& mat = m_materials[a_handle.index];
	if (ImGui::CollapsingHeader(mat.name.c_str()))
	{
		ImGui::PushID(static_cast<int>(a_handle.index));
		ImGui::Indent();

		for (size_t eff_index = 0; eff_index < mat.shader_handle_count; eff_index++)
		{
			ImGui::PushID(static_cast<int>(eff_index));
			ImGuiDisplayShaderEffect(mat.shader_handles[eff_index]);
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
		for (uint32_t i = 0; i < m_materials.size(); i++)
		{
			const MaterialInfo& mat = m_materials[i];
			ImGuiDisplayMaterial(mat.handle);

		}
		ImGui::Unindent();
	}
}

void Editor::ImGuiCreateMaterial()
{
	if (ImGui::CollapsingHeader("create material"))
	{
		static char material_name_buffer[256]{};
		static const ShaderEffectInfo* pvertex_effect = nullptr;
		static const ShaderEffectInfo* pfragment_effect = nullptr;

		ImGui::Indent();

		if (ImGui::Button("generate material"))
		{
			if (pvertex_effect && pfragment_effect && material_name_buffer[0] != '\0')
			{
				CreateMaterialInfo create_material_info;
				create_material_info.name = material_name_buffer; // TEMP
				const ShaderEffectHandle effects[2]{ pvertex_effect->handle, pfragment_effect->handle };
				create_material_info.shader_effects = Slice(effects, _countof(effects));
				this->CreateMaterial(create_material_info);
			}
			else
			{
				BB_WARNING(false, "failed to generate material because the vertex, fragment shader is not selected or the name is empty", WarningType::MEDIUM);
			}
		}

		ImGui::InputText("material name", material_name_buffer, _countof(material_name_buffer));

		if (ImGui::CollapsingHeader("vertex shader"))
		{
			ImGui::Indent();
			if (pvertex_effect != nullptr)
			{
				ImGuiDisplayShaderEffect(pvertex_effect->handle);
			}
			else
			{
				ImGui::Text("no vertex shader selected");
			}

			if (ImGui::CollapsingHeader("select vertex shader"))
			{
				ImGui::Indent();
				for (size_t i = 0; i < m_shader_effects.size(); i++)
				{
					const ShaderEffectInfo& sd_inf = m_shader_effects[i];
					if (sd_inf.shader_stage != SHADER_STAGE::VERTEX)
						continue;
					ImGui::PushID(static_cast<int>(i));
					if (ImGui::Button(sd_inf.name.c_str()))
					{
						pvertex_effect = &sd_inf;
					}
					ImGui::PopID();
				}
				ImGui::Unindent();
			}
			ImGui::Unindent();
		}

		if (ImGui::CollapsingHeader("fragment shader"))
		{
			ImGui::Indent();
			if (pfragment_effect != nullptr)
			{
				ImGuiDisplayShaderEffect(pfragment_effect->handle);
			}
			else
			{
				ImGui::Text("no fragment shader selected");
			}
			if (ImGui::CollapsingHeader("select fragment shader"))
			{
				ImGui::Indent();
				for (size_t i = 0; i < m_shader_effects.size(); i++)
				{
					const ShaderEffectInfo& sd_inf = m_shader_effects[i];
					if (sd_inf.shader_stage != SHADER_STAGE::FRAGMENT_PIXEL)
						continue;
					ImGui::PushID(static_cast<int>(i));
					if (ImGui::Button(sd_inf.name.c_str()))
					{
						pfragment_effect = &sd_inf;
					}
					ImGui::PopID();
				}
				ImGui::Unindent();
			}
			ImGui::Unindent();
		}

		ImGui::Unindent();
	}
}

void Editor::LoadAssetsAsync(MemoryArena&, void* a_params)
{
	LoadAssetsAsync_params* params = reinterpret_cast<LoadAssetsAsync_params*>(a_params);
	MemoryArena load_arena = MemoryArenaCreate();
	//Editor& editor = *params->editor;

	Asset::LoadAssets(load_arena, Slice(params->assets, params->asset_count), params->cmd_list_name);

	// load materials in somehow

	MemoryArenaFree(load_arena);
}
