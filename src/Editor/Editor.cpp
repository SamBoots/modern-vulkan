#include "Editor.hpp"

#include "Program.h"
#include "BBThreadScheduler.hpp"
#include "HID.h"
#include "Math.inl"

#include "imgui.h"

using namespace BB;

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
	Viewport& viewport = *param_in->viewport;
	SceneHierarchy& scene_hierarchy = *param_in->scene_hierarchy;
	RCommandList list = param_in->command_list;

	const RImageView render_target = viewport.StartRenderTarget(list, back_buffer_index);
	scene_hierarchy.DrawSceneHierarchy(list, render_target, back_buffer_index, viewport.GetExtent(), int2());
	viewport.EndRenderTarget(list, back_buffer_index, IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL);
}

void Editor::Init(MemoryArena& a_arena, const WindowHandle a_window, const uint2 a_window_extent, const size_t a_editor_memory)
{
	m_main_window = a_window;
	m_app_window_extent = a_window_extent;

	SetupImGuiInput(a_arena, m_main_window);

	m_gpu_info = GetGPUInfo(a_arena);

	m_editor_allocator.Initialize(a_arena, a_editor_memory);
	void* loaded_model_names = m_editor_allocator.Alloc(EDITOR_MODEL_NAME_ARRAY_SIZE * sizeof(decltype(m_loaded_models_names)::TYPE), 16);

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

	m_imgui_material = Material::GetDefaultMasterMaterial(PASS_TYPE::GLOBAL, MATERIAL_TYPE::MATERIAL_2D);
}

void Editor::Destroy()
{
	DestroyImGuiInput();
	DestroyRenderer();
	DirectDestroyOSWindow(m_main_window);
}

void Editor::StartFrame(const Slice<InputEvent> a_input_events, const float a_delta_time)
{
	m_per_frame.current_count = 0;
	m_swallow_input = false;
	for (size_t i = 0; i < a_input_events.size(); i++)
	{
		const InputEvent& ip = a_input_events[i];
		//imgui can deny our normal input
		if (ImProcessInput(ip))
		{
			m_swallow_input = true;
			continue;
		}

		if (ip.input_type == INPUT_TYPE::KEYBOARD)
		{
			const KeyInfo& ki = ip.key_info;
			float3 cam_move{};
			if (ki.key_pressed)
				switch (ki.scan_code)
				{
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

		}
		else if (ip.input_type == INPUT_TYPE::MOUSE)
		{
			const MouseInfo& mi = ip.mouse_info;
			m_previous_mouse_pos = mi.mouse_pos;

			if (mi.right_released)
				FreezeMouseOnWindow(m_main_window);
			if (mi.left_released)
				UnfreezeMouseOnWindow();

			//if (mi.wheel_move)
			//{
			//	m_cam_speed = Clampf(m_cam_speed + static_cast<float>(mi.wheel_move) * 0.1f,
			//		m_cam_speed_min,
			//		m_cam_speed_max);
			//}
		}
	}

	CommandPool& pool = m_per_frame.pools[0];
	RCommandList& list = m_per_frame.lists[0];
	pool = GetGraphicsCommandPool();
	list = pool.StartCommandList();

	RenderStartFrameInfo start_info;
	start_info.delta_time = a_delta_time;
	start_info.mouse_pos = m_previous_mouse_pos;

	RenderStartFrame(list, start_info, m_per_frame.back_buffer_index);
}

void Editor::EndFrame(MemoryArena& a_arena)
{
	MemoryArenaScope(a_arena)
	{
		if (ImGui::Begin("Editor - Renderer"))
		{
			ImGuiDisplayShaderEffects(a_arena);
			ImGuiDisplayMaterials();
		}
		ImGui::End();

		Asset::ShowAssetMenu(a_arena);
		MainEditorImGuiInfo(a_arena);

		Slice imgui_shaders = Material::GetMaterialShaders(m_imgui_material);
		RenderEndFrame(m_per_frame.lists[0], imgui_shaders[0], imgui_shaders[1], m_per_frame.back_buffer_index);

		for (size_t i = 0; i < m_per_frame.current_count; i++)
		{
			m_per_frame.pools[i].EndCommandList(m_per_frame.lists[i]);
		}

		const uint32_t command_list_count = Max(m_per_frame.current_count.load(), 1u);
		uint64_t present_queue_value;
		// TODO: fence values could bug if no scenes are being rendered.
		PresentFrame(m_per_frame.pools.slice(command_list_count),
			m_per_frame.fences.data(), 
			m_per_frame.fence_values.data(), 
			m_per_frame.current_count,
			present_queue_value);
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

		if (ImGui::CollapsingHeader("Skip render pass option"))
		{
			if (ImGui::Button("toggle skipping skybox pass"))
			{
				a_hierarchy.ToggleSkipSkyboxPass();
			}
			if (ImGui::Button("toggle shadowmapping pass"))
			{
				a_hierarchy.ToggleSkipShadowMappingPass();
			}
			if (ImGui::Button("toggle skipping object rendering pass"))
			{
				a_hierarchy.ToggleSkipObjectRenderingPass();
			}
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

static void ImGuiShowTexturePossibleChange(const RDescriptorIndex a_texture, const float2 a_size, const char* a_name)
{
	if (ImGui::TreeNodeEx(a_name))
	{
		ImGui::Indent();

		ImGui::Image(a_texture.handle, ImVec2(a_size.x, a_size.y));

		ImGui::Unindent();
		ImGui::TreePop();
	}
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
			position_changed = ImGui::InputFloat3("position", transform.m_pos.e);
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

					const MasterMaterial& master = Material::GetMasterMaterial(scene_object.mesh_info.master_material);
					MeshMetallic& metallic = scene_object.mesh_info.material_data;

					ImGui::Text("master Material: %s", master.name.c_str());
					if (ImGui::SliderFloat4("base color factor", metallic.base_color_factor.e, 0.f, 1.f))
						scene_object.mesh_info.material_dirty = true;

					if (ImGui::InputFloat("metallic factor", &metallic.metallic_factor))
						scene_object.mesh_info.material_dirty = true;

					if (ImGui::InputFloat("roughness factor", &metallic.roughness_factor))
						scene_object.mesh_info.material_dirty = true;

					ImGuiShowTexturePossibleChange(metallic.albedo_texture, float2(128, 128), "albedo");
					ImGuiShowTexturePossibleChange(metallic.normal_texture, float2(128, 128), "normal");

					if (ImGui::TreeNodeEx("switch material"))
					{
						ImGui::Indent();

						Slice materials = Material::GetAllMasterMaterials();
						for (size_t i = 0; i < materials.size(); i++)
						{
							const MasterMaterial& new_mat = materials[i];
							if (ImGui::Button(new_mat.name.c_str()))
							{
								Material::FreeMaterialInstance(scene_object.mesh_info.material);
								scene_object.mesh_info.master_material = new_mat.handle;
								scene_object.mesh_info.material = Material::CreateMaterialInstance(scene_object.mesh_info.master_material);
							}
						}

						ImGui::Unindent();
						ImGui::TreePop();
					}

					ImGui::Unindent();
					ImGui::TreePop();
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

				light.pos.x = transform.m_pos.x;
				light.pos.y = transform.m_pos.y;
				light.pos.z = transform.m_pos.z;

				ImGui::InputFloat3("color", light.color.e);
				ImGui::InputFloat("linear radius", &light.radius_linear);
				ImGui::InputFloat("quadratic radius", &light.radius_quadratic);

				switch (static_cast<LIGHT_TYPE>(light.light_type))
				{
				case LIGHT_TYPE::POINT_LIGHT:

					break;
				case LIGHT_TYPE::SPOT_LIGHT:
				case LIGHT_TYPE::DIRECTIONAL_LIGHT:
					ImGui::InputFloat3("direction", light.direction.e);
					break;
				default:
					break;
				}

				if (ImGui::Button("remove light"))
				{
					a_hierarchy.FreeLight(scene_object.light_handle);
					scene_object.light_handle = LightHandle(BB_INVALID_HANDLE_64);
				}

				if (position_changed)
				{
					float4x4& vp = a_hierarchy.m_light_projection_view[scene_object.light_handle];
					const float near_plane = 1.f, far_plane = 7.5f;
					vp = a_hierarchy.CalculateLightProjectionView(transform.m_pos, near_plane, far_plane);
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

void Editor::ImGuiDisplayShaderEffect(MemoryArena& a_temp_arena, const CachedShaderInfo& a_shader_info) const
{
	if (ImGui::CollapsingHeader(a_shader_info.path.c_str()))
	{
		ImGui::Indent();
		ImGui::Text("HANDLE: %u | GENERATION: %u", a_shader_info.handle.index, a_shader_info.handle.extra_index);
		ImGui::Text("ENTRY POINT: %s", a_shader_info.entry.c_str());
		ImGui::Text("SHADER STAGE: %s", ShaderStageToCChar(a_shader_info.stage));
		const StackString<256> next_stages = ShaderStagesToCChar(a_shader_info.next_stages);
		ImGui::Text("NEXT EXPECTED STAGES: %s", next_stages.c_str());

		if (ImGui::Button("Reload Shader"))
		{
			MemoryArenaScope(a_temp_arena)
			{
				const Buffer shader = OSReadFile(a_temp_arena, a_shader_info.path.c_str());
				BB_ASSERT(ReloadShaderEffect(a_shader_info.handle, shader), "something went wrong with reloading a shader");
			}
		}
		ImGui::Unindent();
	}
}

void Editor::ImGuiDisplayShaderEffects(MemoryArena& a_temp_arena)
{
	if (ImGui::CollapsingHeader("shader effects"))
	{
		ImGui::Indent();
		const Slice shaders = Material::GetAllCachedShaders();
		for (size_t i = 0; i < shaders.size(); i++)
		{
			ImGui::PushID(static_cast<int>(i));
			// rework this?
			ImGuiDisplayShaderEffect(a_temp_arena, shaders[i]);
			ImGui::PopID();
		}
		ImGui::Unindent();
	}
}

void Editor::ImGuiDisplayMaterial(const MasterMaterial& a_material) const
{
	if (ImGui::CollapsingHeader(a_material.name.c_str()))
	{
		ImGui::Indent();

		for (size_t eff_index = 0; eff_index < a_material.shader_effect_count; eff_index++)
		{
			ImGui::PushID(static_cast<int>(eff_index));

			ImGui::PopID();
		}

		ImGui::Text("Material CPU writeable: %d", a_material.cpu_writeable);
		ImGui::Separator();

		ImGui::Text("Descriptor Layout 2 (scene): %s", Material::PASS_TYPE_STR(a_material.pass_type));
		ImGui::Text("Descriptor Layout 3 (material): %s", Material::MATERIAL_TYPE_STR(a_material.material_type));
		ImGui::Text("Descriptor Layout 4 (mesh): %s", "not implemented yet");

		ImGui::Unindent();
	}
}

void Editor::ImGuiDisplayMaterials()
{
	if (ImGui::CollapsingHeader("materials"))
	{
		ImGui::Indent();
		const Slice materials = Material::GetAllMasterMaterials();
		for (uint32_t i = 0; i < materials.size(); i++)
		{
			ImGui::PushID(static_cast<int>(i));
			ImGuiDisplayMaterial(materials[i]);
			ImGui::PopID();
		}
		ImGui::Unindent();
	}
}
