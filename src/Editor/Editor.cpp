#include "Editor.hpp"

#include "Program.h"
#include "BBThreadScheduler.hpp"
#include "Math/Math.inl"
#include "Engine.hpp"

#include "ProfilerWindow.hpp"

#include "ImGuiImpl.hpp"
#include "imgui.h"

#include "InputSystem.hpp"
#include "Math/Collision.inl"

using namespace BB;

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
				ImGui::Text("heap size: %llu", a_gpu_info.memory_heaps[i].heap_size);
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

void Editor::MainEditorImGuiInfo(const MemoryArena& a_arena) const
{
	if (ImGui::CollapsingHeader("general engine info"))
	{
        ImGui::Indent();
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
        ImGui::Unindent();
	}
}

void Editor::ThreadFuncForDrawing(MemoryArena&, void* a_param)
{
	ThreadFuncForDrawing_Params* params = reinterpret_cast<ThreadFuncForDrawing_Params*>(a_param);
	params->editor.UpdateGame(params->instance, params->delta_time);
}

void Editor::UpdateGame(EditorGame& a_instance, const float a_delta_time)
{
	const uint32_t pool_index = m_per_frame.current_count.fetch_add(1, std::memory_order_relaxed);

	CommandPool& pool = m_per_frame.pools[pool_index];
	RCommandList& list = m_per_frame.lists[pool_index];
	// render_pool 0 is already set.
	if (pool_index != 0)
	{
		pool = GetGraphicsCommandPool();
		list = pool.StartCommandList();
	}

    CameraInput input;
    input.channel = m_input.channel;
    input.move = m_input.camera_move;
    input.move_speed_slider = m_input.move_speed_slider;
    input.look_around = m_input.look_around;
    input.enable_rotate = m_input.enable_rotate;

    Viewport& viewport = a_instance.GetGameInstance().GetViewport();
    SceneHierarchy& hierarchy = a_instance.GetGameInstance().GetSceneHierarchy();
    hierarchy.StartFrame();
    bool success;
    if (!m_swallow_input && viewport.PositionWithinViewport(uint2(static_cast<unsigned int>(m_previous_mouse_pos.x), static_cast<unsigned int>(m_previous_mouse_pos.y))))
    {
        success = a_instance.Update(a_delta_time, Input::InputActionIsPressed(m_input.channel, m_input.toggle_editor_cam), input, true);
        if (a_instance.IsEditorMode())
            UpdateGizmo(viewport, hierarchy, a_instance.GetCameraPos());
    }
    else
    {
        success = a_instance.Update(a_delta_time, false, input, false);
    }

    const float3 pos = a_instance.GetCameraPos();
    const float4x4 view = a_instance.GetCameraView();
    a_instance.GetSceneHierarchy().GetECS().GetRenderSystem().SetView(view, pos);
    
	const SceneFrame frame = hierarchy.UpdateScene(list, viewport, );
	// book keep all the end details
	m_per_frame.draw_struct[pool_index].type = DRAW_TYPE::GAME;
    m_per_frame.draw_struct[pool_index].game = &a_instance;
	m_per_frame.fences[pool_index] = frame.render_frame.fence;
	m_per_frame.fence_values[pool_index] = frame.render_frame.fence_value;
	m_per_frame.frame_results[pool_index] = frame;
    m_per_frame.success[pool_index] = success;
    if (!success)
        m_per_frame.error_message[pool_index] = StackString<64>("Game Instance: Lua is dirty!");
}

void Editor::UpdateGizmo(Viewport& a_viewport, SceneHierarchy& a_scene_hierarchy, const float3 a_cam_pos)
{
    if (Input::InputActionIsPressed(m_input.channel, m_input.click_on_screen))
    {
        float2 mouse_pos_window;
        if (a_viewport.ScreenToViewportMousePosition(m_previous_mouse_pos, mouse_pos_window))
        {
            const float4x4 view = a_scene_hierarchy.GetECS().GetRenderSystem().GetView();
            const float3 dir = ScreenToWorldRaycast(mouse_pos_window, a_viewport.GetExtent(), a_scene_hierarchy.GetECS().GetRenderSystem().GetProjection(), view);
            if (m_gizmo.HasValidEntity())
            {
                if (!m_gizmo.RayCollide(a_cam_pos, dir))
                { 
                    const ECSEntity entity = a_scene_hierarchy.GetECS().SelectEntityByRay(a_cam_pos, dir);
                    if (entity.IsValid())
                        m_gizmo.SelectEntity(entity, &a_scene_hierarchy.GetECS());
                    else
                        m_gizmo.UnselectEntity();
                }
            }
            else
            {
                const ECSEntity entity = a_scene_hierarchy.GetECS().SelectEntityByRay(a_cam_pos, dir);
                if (entity.IsValid())
                    m_gizmo.SelectEntity(entity, &a_scene_hierarchy.GetECS());
            }
        }
    }

    if (Input::InputActionIsReleased(m_input.channel, m_input.click_on_screen))
        m_gizmo.ClearCollision();

    if (Input::InputActionIsPressed(m_input.channel, m_input.gizmo_toggle_scale))
        m_gizmo.SetMode(GIZMO_MODE::SCALE);
    if (Input::InputActionIsReleased(m_input.channel, m_input.gizmo_toggle_scale))
        m_gizmo.SetMode(GIZMO_MODE::TRANSLATE);

    if (m_gizmo.HasValidEntity())
    { 
        m_gizmo.Draw();
        const float2 mouse_move = Input::InputActionGetFloat2(m_input.channel, m_input.mouse_move);
        m_gizmo.ManipulateEntity(mouse_move);
    }
}

void Editor::Init(MemoryArena& a_arena, const EditorCreateInfo& a_create_info)
{
	// console stuff
	ConsoleCreateInfo create_info;
	create_info.entries_till_resize = 8;
	create_info.entries_till_file_write = 8;
	create_info.write_to_console = true;
	create_info.write_to_file = true;
	create_info.enabled_warnings = WARNING_TYPES_ALL;
	m_console.Init(create_info);

	m_main_window = a_create_info.window;
	m_app_window_extent = a_create_info.window_extent;

	const bool success = ImInit(a_arena, m_main_window);
	BB_ASSERT(success, "failed to init imgui");

	m_gpu_info = GetGPUInfo(a_arena);

	m_imgui_material = Material::GetDefaultMasterMaterial(PASS_TYPE::GLOBAL, MATERIAL_TYPE::MATERIAL_2D);

	const uint32_t frame_count = GetBackBufferCount();

	ImageCreateInfo render_target_info;
	render_target_info.name = "image before transfer to swapchain";
	render_target_info.width = m_app_window_extent.x;
	render_target_info.height = m_app_window_extent.y;
	render_target_info.depth = 1;
	render_target_info.array_layers = static_cast<uint16_t>(frame_count);
	render_target_info.mip_levels = 1;
	render_target_info.type = IMAGE_TYPE::TYPE_2D;
	render_target_info.use_optimal_tiling = true;
	render_target_info.is_cube_map = false;
	render_target_info.format = IMAGE_FORMAT::RGBA16_SFLOAT;
	render_target_info.usage = IMAGE_USAGE::SWAPCHAIN_COPY_IMG;

	m_render_target = CreateImage(render_target_info);

	for (size_t i = 0; i < frame_count; i++)
	{
		ImageViewCreateInfo view_info;
		view_info.name = "editor window";
		view_info.image = m_render_target;
		view_info.array_layers = 1;
		view_info.base_array_layer = static_cast<uint16_t>(i);
		view_info.mip_levels = 1;
		view_info.base_mip_level = 0;
		view_info.aspects = IMAGE_ASPECT::COLOR;
		view_info.type = IMAGE_VIEW_TYPE::TYPE_2D;
		view_info.format = IMAGE_FORMAT::RGBA16_SFLOAT;

		m_render_target_descs[i] = CreateImageView(view_info);
	}
    m_input.channel = Input::CreateInputChannel(a_arena, "editor", 8);
    {
        InputActionCreateInfo action_info;
        action_info.binding_type = INPUT_BINDING_TYPE::BINDING;
        action_info.value_type = INPUT_VALUE_TYPE::BOOL;
        action_info.source = INPUT_SOURCE::MOUSE;
        action_info.input_keys[0].mouse_input = MOUSE_INPUT::LEFT_BUTTON;
        m_input.click_on_screen = Input::CreateInputAction(m_input.channel, "click on screen", action_info);
    }
    {
        InputActionCreateInfo input_create{};
        input_create.value_type = INPUT_VALUE_TYPE::FLOAT_2;
        input_create.binding_type = INPUT_BINDING_TYPE::BINDING;
        input_create.source = INPUT_SOURCE::MOUSE;
        input_create.input_keys[0].mouse_input = MOUSE_INPUT::MOUSE_MOVE;
        m_input.mouse_move = Input::CreateInputAction(m_input.channel, "mouse move", input_create);
    }
    {
        InputActionCreateInfo input_create{};
        input_create.value_type = INPUT_VALUE_TYPE::BOOL;
        input_create.binding_type = INPUT_BINDING_TYPE::BINDING;
        input_create.source = INPUT_SOURCE::KEYBOARD;
        input_create.input_keys[0].keyboard_key = KEYBOARD_KEY::CONTROLLEFT;
        m_input.gizmo_toggle_scale = Input::CreateInputAction(m_input.channel, "gizmo toggle scale", input_create);
    }
    {
        InputActionCreateInfo input_create{};
        input_create.value_type = INPUT_VALUE_TYPE::BOOL;
        input_create.binding_type = INPUT_BINDING_TYPE::BINDING;
        input_create.source = INPUT_SOURCE::KEYBOARD;
        input_create.input_keys[0].keyboard_key = KEYBOARD_KEY::G;
        m_input.toggle_editor_cam = Input::CreateInputAction(m_input.channel, "toggle editor camera", input_create);
    }

    {
        InputActionCreateInfo input_create{};
        input_create.value_type = INPUT_VALUE_TYPE::FLOAT_2;
        input_create.binding_type = INPUT_BINDING_TYPE::AXIS_2D;
        input_create.source = INPUT_SOURCE::KEYBOARD;
        input_create.input_keys[0].keyboard_key = KEYBOARD_KEY::W;
        input_create.input_keys[1].keyboard_key = KEYBOARD_KEY::S;
        input_create.input_keys[2].keyboard_key = KEYBOARD_KEY::D;
        input_create.input_keys[3].keyboard_key = KEYBOARD_KEY::A;
        m_input.camera_move = Input::CreateInputAction(m_input.channel, "camera move", input_create);
    }
    {
        InputActionCreateInfo input_create{};
        input_create.value_type = INPUT_VALUE_TYPE::FLOAT;
        input_create.binding_type = INPUT_BINDING_TYPE::BINDING;
        input_create.source = INPUT_SOURCE::MOUSE;
        input_create.input_keys[0].mouse_input = MOUSE_INPUT::SCROLL_WHEEL;
        m_input.move_speed_slider = Input::CreateInputAction(m_input.channel, "move speed slider", input_create);
    }
    {
        InputActionCreateInfo input_create{};
        input_create.value_type = INPUT_VALUE_TYPE::FLOAT_2;
        input_create.binding_type = INPUT_BINDING_TYPE::BINDING;
        input_create.source = INPUT_SOURCE::MOUSE;
        input_create.input_keys[0].mouse_input = MOUSE_INPUT::MOUSE_MOVE;
        m_input.look_around = Input::CreateInputAction(m_input.channel, "look around", input_create);
    }
    {
        InputActionCreateInfo input_create{};
        input_create.value_type = INPUT_VALUE_TYPE::BOOL;
        input_create.binding_type = INPUT_BINDING_TYPE::BINDING;
        input_create.source = INPUT_SOURCE::MOUSE;
        input_create.input_keys[0].mouse_input = MOUSE_INPUT::RIGHT_BUTTON;
        m_input.enable_rotate = Input::CreateInputAction(m_input.channel, "enable rotate", input_create);
    }

    m_game_instances.Init(a_arena, a_create_info.game_instance_max);
    for (size_t i = 0; i < a_create_info.initial_games.size(); i++)
    {
        AddGameInstance(a_create_info.initial_games[i].dir_name, a_create_info.initial_games[i].register_funcs);
    }
}

void Editor::Destroy()
{
	ImShutdown();
	DestroyRenderer();
	DirectDestroyOSWindow(m_main_window);
}

void Editor::StartFrame(MemoryArena& a_arena, const Slice<InputEvent> a_input_events, const float a_delta_time)
{
	ImNewFrame(m_app_window_extent);

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
	}

	CommandPool& pool = m_per_frame.pools[0];
	RCommandList& list = m_per_frame.lists[0];
	pool = GetGraphicsCommandPool();
	list = pool.StartCommandList();

	RenderStartFrameInfo start_info;
	start_info.delta_time = a_delta_time;
	start_info.mouse_pos = m_previous_mouse_pos;
    m_previous_mouse_pos = Input::GetMousePos(m_main_window);

	RenderStartFrame(list, start_info, m_render_target, m_per_frame.back_buffer_index);
	m_per_frame.current_count = 0;

	m_console.ImGuiShowConsole(a_arena, m_app_window_extent);
}

void Editor::UpdateGames(MemoryArena& a_arena, const float a_delta_time)
{
    ThreadTask* tasks = ArenaAllocArr(a_arena, ThreadTask, m_game_instances.size());

    for (uint32_t i = 0; i < m_game_instances.size(); i++)
        tasks[i] = UpdateGameInstance(a_delta_time, m_game_instances[i]);

    for (uint32_t i = 0; i < m_game_instances.size(); i++)
        Threads::WaitForTask(tasks[i]);
}


void Editor::AddGameInstance(const StringView a_dir_path, const ConstSlice<PFN_LuaPluginRegisterFunctions> a_register_funcs)
{
    const uint32_t index = m_game_instances.size();
    m_game_instances.emplace_back();
    m_game_instances[index].Init(m_app_window_extent / 2, a_dir_path, nullptr, a_register_funcs);
}

ThreadTask Editor::UpdateGameInstance(const float a_delta_time, class EditorGame& a_game)
{
    ThreadFuncForDrawing_Params params =
    {
        *this,
        a_delta_time,
        a_game
    };

    ImGuiDisplayGame(a_game.GetGameInstance());

    return Threads::StartTaskThread(ThreadFuncForDrawing, &params, sizeof(params), L"scene draw task");
}

void Editor::EndFrame(MemoryArena& a_arena)
{
	bool skip = false;
	MemoryArenaScope(a_arena)
	{
        ImGuiDisplayEditor(a_arena);

		for (size_t i = 0; i < m_per_frame.current_count; i++)
		{
            DrawStruct& ds = m_per_frame.draw_struct[i];
            if (ds.type == DRAW_TYPE::GAME)
            {
                EditorGame& game_inst = *ds.game;
                if (m_per_frame.success[i])
                {
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                    if (ImGui::Begin(game_inst.GetSceneHierarchy().GetECS().GetName().c_str(), nullptr, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollbar))
                    {
                        if (ImGui::BeginMenuBar())
                        {
                            if (ImGui::BeginMenu("screenshot"))
                            {
                                static char image_name[128]{};
                                ImGui::InputText("sceenshot name", image_name, 128);

                                if (ImGui::Button("make screenshot"))
                                    game_inst.GetSceneHierarchy().GetECS().GetRenderSystem().Screenshot(image_name);

                                ImGui::EndMenu();
                            }
                            if (ImGui::Button("reload lua"))
                                game_inst.Reload();
                            ImGui::EndMenuBar();
                        }
                        DrawImgui(m_per_frame.frame_results[i].render_frame.render_target, game_inst.GetViewport());
                    }
                    ImGui::PopStyleVar();
                }
                else
                {
                    if (ImGui::Begin(game_inst.GetSceneHierarchy().GetECS().GetName().c_str()))
                    {
                        ImGui::TextUnformatted(m_per_frame.error_message[i].c_str());
                        if (ImGui::Button("Reload lua"))
                            game_inst.Reload();
                    }
                }
                ImGui::End();
            }
		}

		// CURFRAME = the render internal frame
		ImRenderFrame(m_per_frame.lists[0], GetImageView(m_render_target_descs[m_per_frame.back_buffer_index]), m_app_window_extent, true, m_imgui_material);
		ImGui::EndFrame();

		PRESENT_IMAGE_RESULT result = RenderEndFrame(m_per_frame.lists[0], m_render_target, m_per_frame.back_buffer_index);
		if (result == PRESENT_IMAGE_RESULT::SWAPCHAIN_OUT_OF_DATE)
		{
			skip = true;
		}


		for (size_t i = 0; i < m_per_frame.current_count; i++)
		{
			m_per_frame.pools[i].EndCommandList(m_per_frame.lists[i]);
		}

		const uint32_t command_list_count = Max(m_per_frame.current_count.load(), 1u);
		uint64_t present_queue_value;
		// TODO: fence values could bug if no scenes are being rendered.
		result = PresentFrame(m_per_frame.pools.slice(command_list_count),
			m_per_frame.fences.data(), 
			m_per_frame.fence_values.data(), 
			m_per_frame.current_count,
			present_queue_value, 
			skip);
	}
}

bool Editor::ResizeWindow(const uint2 a_window)
{
	GPUWaitIdle();

	m_app_window_extent = a_window;
	FreeImage(m_render_target);

	ImageCreateInfo render_target_info;
	render_target_info.name = "image before transfer to swapchain";
	render_target_info.width = m_app_window_extent.x;
	render_target_info.height = m_app_window_extent.y;
	render_target_info.depth = 1;
	render_target_info.array_layers = static_cast<uint16_t>(m_render_target_descs.size());
	render_target_info.mip_levels = 1;
	render_target_info.type = IMAGE_TYPE::TYPE_2D;
	render_target_info.use_optimal_tiling = true;
	render_target_info.is_cube_map = false;
	render_target_info.format = IMAGE_FORMAT::RGBA16_SFLOAT;
	render_target_info.usage = IMAGE_USAGE::SWAPCHAIN_COPY_IMG;

	m_render_target = CreateImage(render_target_info);

	for (size_t i = 0; i < m_render_target_descs.size(); i++)
	{
		FreeImageView(m_render_target_descs[i]);

		ImageViewCreateInfo view_info;
		view_info.name = "image before transfer to swapchain";
		view_info.image = m_render_target;
		view_info.array_layers = 1;
		view_info.base_array_layer = static_cast<uint16_t>(i);
		view_info.mip_levels = 1;
		view_info.base_mip_level = 0;
		view_info.aspects = IMAGE_ASPECT::COLOR;
		view_info.type = IMAGE_VIEW_TYPE::TYPE_2D;
		view_info.format = IMAGE_FORMAT::RGBA16_SFLOAT;

		m_render_target_descs[i] = CreateImageView(view_info);
	}

	ResizeSwapchain(m_app_window_extent);

	return true;
}

bool Editor::DrawImgui(const RDescriptorIndex a_render_target, Viewport& a_viewport)
{
	bool rendered_image = false;
	ImGuiIO im_io = ImGui::GetIO();

	constexpr uint2 MINIMUM_WINDOW_SIZE = uint2(80, 80);

	const ImVec2 viewport_offset = ImGui::GetWindowPos();
	const ImVec2 viewport_draw_area = ImGui::GetContentRegionAvail();
    const ImVec2 viewport_size = ImGui::GetWindowSize();
    const uint2 window_size_u = uint2(static_cast<unsigned int>(viewport_size.x), static_cast<unsigned int>(viewport_size.y));
	if (window_size_u.x < MINIMUM_WINDOW_SIZE.x || window_size_u.y < MINIMUM_WINDOW_SIZE.y)
		return false;

	if (window_size_u != a_viewport.GetExtent() && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
		a_viewport.SetExtent(window_size_u);

	a_viewport.SetOffset(int2(static_cast<int>(viewport_offset.x), static_cast<int>(viewport_offset.y)));

	ImGui::Image(a_render_target.handle, viewport_draw_area);
	rendered_image = true;

	return rendered_image;
}

void Editor::ImGuiDisplayEditor(MemoryArena& a_arena)
{
    if (ImGui::Begin("editor"))
    {
        MainEditorImGuiInfo(a_arena);
        ImGuiDisplayShaderEffects(a_arena);
        ImGuiDisplayMaterials();
        ImGuiDisplayInputChannel(m_input.channel);
        ImGuiShowProfiler(a_arena);
        ImGuiDisplayGames();

        for (uint32_t i = 0; i < 100; i++)
        {
            ImGui::Text("%u", i);
            ImGui::Image(i, ImVec2(160.f, 160.f));
        }

    }
    ImGui::End();


    Asset::ShowAssetMenu(a_arena);
}

void Editor::ImGuiDisplayGame(GameInstance& a_game)
{
    StackString<128> name_editor = a_game.m_project_name.GetView();
    name_editor.append(" - editor");
    if (ImGui::Begin(name_editor.c_str()))
    {
        ImguiDisplayECS(a_game.GetSceneHierarchy().m_ecs, a_game.GetViewport().GetExtent());
        ImGuiDisplayInputChannel(a_game.GetInputChannel());
    }
    ImGui::End();
}

void Editor::ImguiDisplayECS(EntityComponentSystem& a_ecs, const uint2 a_viewport_extent)
{
    if (ImGui::CollapsingHeader("ECS"))
	{
		ImGui::Indent();

		ImGuiCreateEntity(a_ecs);

		RenderSystem& render_sys = a_ecs.GetRenderSystem();

		if (ImGui::Button("GAMER MODE"))
		{
            BB_UNIMPLEMENTED("no fx changing option");
			//auto& postfx_options = render_sys.m_postfx;
			//postfx_options.bloom_strength = 5.f;
			//postfx_options.bloom_scale = 10.5f;
		}

        if (ImGui::CollapsingHeader("ECS"))
        {
            ImGui::Indent();

            RenderOptions r_o = render_sys.GetOptions();

            {
                int resolution_index = 0;
                FixedArray<char[32], _countof(RENDER_OPTIONS::RESOLUTIONS)> names{};

                for (size_t i = 0; i < names.size(); i++)
                    if (r_o.resolution == RENDER_OPTIONS::RESOLUTIONS[i])
                    {
                        resolution_index = static_cast<int>(i);
                        break;
                    }

                FixedArray<char*, names.size()> names_list{};
				// temp make this constexpr or something

                for (size_t i = 0; i < names.size(); i++)
                {
					FormatString(names[i], _countof(names[i]), "%uX%u", RENDER_OPTIONS::RESOLUTIONS[i].x, RENDER_OPTIONS::RESOLUTIONS[i].y);
                    names_list[i] = names[i];
                }

                ImGui::Text("Resolution: %u x %u", r_o.resolution.x, r_o.resolution.y);
                if (ImGui::Combo("Resolution", &resolution_index, names_list.data(), names.size()))
                {
                    r_o.resolution = RENDER_OPTIONS::RESOLUTIONS[resolution_index];
                }

            }

            render_sys.SetOptions(r_o);

            ImGui::Unindent();
        }

		if (ImGui::CollapsingHeader("graphical options"))
		{
			ImGui::InputFloat("exposure", &render_sys.m_graph_system.GetGlobalData().scene_info.exposure);
			ImGui::InputFloat3("ambient light", render_sys.m_graph_system.GetGlobalData().scene_info.ambient_light.e);
		}

		if (ImGui::CollapsingHeader("post fx option"))
		{
            BB_UNIMPLEMENTED("no fx changing option");
			//auto& postfx_options = render_sys.m_postfx;
			//ImGui::InputFloat("bloom strength", &postfx_options.bloom_strength);
			//ImGui::InputFloat("bloom scale", &postfx_options.bloom_scale);
		}

		for (uint32_t i = 0; i < a_ecs.m_root_entity_system.root_entities.Size(); i++)
		{
			const ECSEntity entity = a_ecs.m_root_entity_system.root_entities[i];
			ImGui::PushID(static_cast<int>(entity.index));

			ImGuiDisplayEntity(a_ecs, entity, a_viewport_extent);

			ImGui::PopID();
		}

		ImGui::Unindent();
	}
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

void Editor::ImGuiDisplayEntity(EntityComponentSystem& a_ecs, const ECSEntity a_entity, const uint2 a_viewport_extent)
{
	ImGui::PushID(static_cast<int>(a_entity.handle));

	if (ImGui::CollapsingHeader(a_ecs.m_name_pool.GetComponent(a_entity).c_str()))
	{
		ImGui::Indent();
		bool position_changed = false;
		
		if (ImGui::TreeNodeEx("transform"))
		{
			float3& pos = a_ecs.m_positions.GetComponent(a_entity);
			//float3x3& rot = a_ecs.m_rotations.GetComponent(a_entity);
			float3& scale = a_ecs.m_scales.GetComponent(a_entity);

			if (ImGui::InputFloat3("position", pos.e))
			{
				position_changed = true;
				a_ecs.m_transform_system.dirty_transforms.Insert(a_entity);
			}
			float3 euler_angles;
			if (ImGui::InputFloat3("rotation", euler_angles.e))
			{
				BB_UNIMPLEMENTED("rotation float3x3->euler->float3x3");
				a_ecs.m_transform_system.dirty_transforms.Insert(a_entity);
			}
			if (ImGui::InputFloat3("scale", scale.e))
			{
				a_ecs.m_transform_system.dirty_transforms.Insert(a_entity);
			}

			ImGui::TreePop();
		}

		const float3 pos = a_ecs.m_positions.GetComponent(a_entity);

		if (a_ecs.m_ecs_entities.HasSignature(a_entity, RENDER_ECS_SIGNATURE))
		{
			RenderComponent& RenderComponent = a_ecs.m_render_mesh_pool.GetComponent(a_entity);
			if (ImGui::TreeNodeEx("rendering"))
			{
				ImGui::Indent();

				if (ImGui::TreeNodeEx("material"))
				{
					ImGui::Indent();

					const MasterMaterial& master = Material::GetMasterMaterial(RenderComponent.master_material);
					MeshMetallic& metallic = RenderComponent.material_data;

					ImGui::Text("master Material: %s", master.name.c_str());
					if (ImGui::SliderFloat4("base color factor", metallic.base_color_factor.e, 0.f, 1.f))
						RenderComponent.material_dirty = true;

					if (ImGui::InputFloat("metallic factor", &metallic.metallic_factor))
						RenderComponent.material_dirty = true;

					if (ImGui::InputFloat("roughness factor", &metallic.roughness_factor))
						RenderComponent.material_dirty = true;

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
								Material::FreeMaterialInstance(RenderComponent.material);
								RenderComponent.master_material = new_mat.handle;
								RenderComponent.material = Material::CreateMaterialInstance(RenderComponent.master_material);
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

		if (a_ecs.m_ecs_entities.HasSignature(a_entity, LIGHT_ECS_SIGNATURE))
		{
			if (ImGui::TreeNodeEx("light"))
			{
				ImGui::Indent();
				LightComponent& comp = a_ecs.m_light_pool.GetComponent(a_entity);

				comp.light.pos.x = pos.x;
				comp.light.pos.y = pos.y;
				comp.light.pos.z = pos.z;

				ImGui::InputFloat3("color", comp.light.color.e);
				ImGui::InputFloat("linear radius", &comp.light.radius_linear);
				ImGui::InputFloat("quadratic radius", &comp.light.radius_quadratic);

				switch (static_cast<LIGHT_TYPE>(comp.light.light_type))
				{
				case LIGHT_TYPE::POINT_LIGHT:

					break;
				case LIGHT_TYPE::SPOT_LIGHT:
				case LIGHT_TYPE::DIRECTIONAL_LIGHT:
					ImGui::InputFloat3("direction", comp.light.direction.e);
					break;
				default:
					break;
				}

				if (ImGui::Button("remove light"))
					a_ecs.EntityFreeLight(a_entity);

				if (position_changed)
				{
					const float near_plane = 1.f, far_plane = 7.5f;
					comp.projection_view =SceneHierarchy::CalculateLightProjectionView(pos, float3(comp.light.direction.x, comp.light.direction.y, comp.light.direction.z), a_viewport_extent, near_plane, far_plane);
				}

				ImGui::Unindent();
				ImGui::TreePop();
			}
		}

		if (ImGui::TreeNodeEx("Add Component"))
		{
			ImGui::Indent();
			if (!a_ecs.m_ecs_entities.HasSignature(a_entity, LIGHT_ECS_SIGNATURE))
			{
				if (ImGui::TreeNodeEx("Light"))
				{
					ImGui::Indent();

					static float specular_strength = 0.5f;
					static float3 color = float3(1.f);
					static float cutoff_radius = 0.3f;
					static float3 direction = float3(0.f);

					static float constant = 1.f;
					static float linear = 0.35f;
					static float quadratic = 0.5f;

					ImGui::InputFloat3("color", color.e);
					ImGui::InputFloat3("direction", direction.e);
					ImGui::InputFloat("specular strength", &specular_strength);
					ImGui::InputFloat("cutoff radius", &cutoff_radius);
					ImGui::InputFloat("radius constant", &constant);
					ImGui::InputFloat("radius linear", &linear);
					ImGui::InputFloat("radius quadratic", &quadratic);

					if (ImGui::Button("create light"))
					{
						LightComponent light_comp;
						light_comp.light.light_type = static_cast<uint32_t>(LIGHT_TYPE::DIRECTIONAL_LIGHT);
						light_comp.light.color = float4(color.x, color.y, color.z, specular_strength);
						light_comp.light.pos = float4(pos.x, pos.y, pos.z, 0.0f);
						light_comp.light.direction = float4(direction.x, direction.y, direction.z, cutoff_radius);

						light_comp.light.radius_constant = constant;
						light_comp.light.radius_linear = linear;
						light_comp.light.radius_quadratic = quadratic;

						const float near_plane = 1.f, far_plane = 7.5f;
						light_comp.projection_view = SceneHierarchy::CalculateLightProjectionView(pos, direction, a_viewport_extent, near_plane, far_plane);

						a_ecs.EntityAssignLight(a_entity, light_comp);
					}

					ImGui::Unindent();
					ImGui::TreePop();
				}
			}
			ImGui::Unindent();
			ImGui::TreePop();
		}

		const EntityRelation relation = a_ecs.m_relations.GetComponent(a_entity);
		ECSEntity child = relation.first_child;
		for (size_t i = 0; i < relation.child_count; i++)
		{
			ImGuiDisplayEntity(a_ecs, child, a_viewport_extent);
			child = a_ecs.m_relations.GetComponent(child).next;
		}

        ImGuiCreateEntity(a_ecs, a_entity);

        if (ImGui::Button("Destroy entity"))
            a_ecs.DestroyEntity(a_entity);
            
		ImGui::Unindent();
	}

	ImGui::PopID();
}

void Editor::ImGuiCreateEntity(EntityComponentSystem& a_ecs, const ECSEntity a_parent)
{
	if (ImGui::TreeNodeEx("create scene object menu"))
	{
		ImGui::Indent();
		static char mesh_name[NameComponent::capacity()];

		ImGui::InputText("entity name: ", mesh_name, NameComponent::capacity());

		if (ImGui::Button("create scene object"))
		{
			a_ecs.CreateEntity(mesh_name, a_parent);
		}

		ImGui::Unindent();
		ImGui::TreePop();
	}
}

constexpr int RELOAD_STATUS_OK = 0;
constexpr int RELOAD_STATUS_SUCCESS = 1;
constexpr int RELOAD_STATUS_FAIL = 2;

constexpr const char* RELOAD_STATUS_TEXT[] =
{
	"shader reload status: ok",
	"shader reload status: compilation succeeded",
	"shader reload status: compilation failed"
};

constexpr ImVec4 RELOAD_STATUS_COLOR[] =
{
	ImVec4(1.f, 1.f, 1.f, 1.f),
	ImVec4(0.f, 1.f, 0.f, 1.f),
	ImVec4(1.f, 0.f, 0.f, 1.f),
};

void Editor::ImGuiDisplayShaderEffect(MemoryArenaTemp a_temp_arena, const CachedShaderInfo& a_shader_info, int& a_reload_status) const
{

	if (ImGui::CollapsingHeader(a_shader_info.path.c_str()))
	{
		ImGui::Indent();
		ImGui::Text("HANDLE: %u | GENERATION: %u", a_shader_info.handle.index, a_shader_info.handle.extra_index);
		ImGui::Text("ENTRY POINT: %s", a_shader_info.entry.c_str());
		ImGui::Text("SHADER STAGE: %s", ShaderStageToCChar(a_shader_info.stage));
		const StackString<256> next_stages = ShaderStagesToCChar(a_shader_info.next_stages);
		ImGui::Text("NEXT EXPECTED STAGES: %s", next_stages.c_str());

		ImGui::TextColored(RELOAD_STATUS_COLOR[a_reload_status], "%s", RELOAD_STATUS_TEXT[a_reload_status]);
		if (ImGui::Button("Reload Shader"))
		{
			const Buffer shader = OSReadFile(a_temp_arena, a_shader_info.path.c_str());
			a_reload_status = ReloadShaderEffect(a_shader_info.handle, shader) ? RELOAD_STATUS_SUCCESS : RELOAD_STATUS_FAIL;
		}

		ImGui::Unindent();
	}
}

void Editor::ImGuiDisplayShaderEffects(MemoryArena& a_arena)
{
	static int reload_status = RELOAD_STATUS_OK;
	if (ImGui::CollapsingHeader("shader effects"))
	{
		ImGui::Indent();
		const Slice shaders = Material::GetAllCachedShaders();
		for (size_t i = 0; i < shaders.size(); i++)
		{
			ImGui::PushID(static_cast<int>(i));
			ImGuiDisplayShaderEffect(a_arena, shaders[i], reload_status);
			ImGui::PopID();
		}
		ImGui::Unindent();
	}
	else
		reload_status = RELOAD_STATUS_OK;
}

static void DisplayShader(const ShaderEffectHandle a_handle, const char* a_shader_name)
{
    if (a_handle.IsValid())
    {
        ImGui::TextUnformatted(a_shader_name);
    }
}

void Editor::ImGuiDisplayMaterial(const MasterMaterial& a_material) const
{
	if (ImGui::CollapsingHeader(a_material.name.c_str()))
	{
		ImGui::Indent();

        DisplayShader(a_material.shaders.vertex, "vertex");
        DisplayShader(a_material.shaders.fragment_pixel, "fragment_pixel");
        DisplayShader(a_material.shaders.geometry, "geometry");

		ImGui::Text("Material CPU writeable: %d", a_material.cpu_writeable);
		ImGui::Separator();

		ImGui::Text("Descriptor Layout 2 (scene): %s", Material::PASS_TYPE_STR(a_material.pass_type));
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

static inline KEYBOARD_KEY KeyboardKeyChange(const char* a_text, const KEYBOARD_KEY a_current_key)
{
    uint32_t selected = static_cast<uint32_t>(a_current_key);

    if (ImGui::BeginCombo(a_text, KEYBOARD_KEY_STR(a_current_key))) {
        for (uint32_t i = 0; i < static_cast<uint32_t>(KEYBOARD_KEY::ENUM_SIZE); i++) {
            const KEYBOARD_KEY key = static_cast<KEYBOARD_KEY>(i);
            const bool is_selected = a_current_key == key;
            if (ImGui::Selectable(KEYBOARD_KEY_STR(key), is_selected)) {
                selected = i;
            }
            // Set initial focus when opening combo
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    return static_cast<KEYBOARD_KEY>(selected);
}

void Editor::ImGuiDisplayInputChannel(const InputChannelHandle a_channel)
{
    if (ImGui::CollapsingHeader("input channel"))
    {
        ImGui::Indent();
        Slice ipas = Input::GetAllInputActions(a_channel);
        for (uint32_t i = 0; i < ipas.size(); i++)
        {
            InputAction& ipa = ipas[i];
            ImGui::PushID(static_cast<int>(i));

            if (ImGui::TreeNode(ipa.name.c_str()))
            {
                ImGui::Indent();

                ImGui::Text("Source: %s", INPUT_SOURCE_STR(ipa.input_source));
                ImGui::Text("Binding Type: %s", INPUT_BINDING_TYPE_STR(ipa.binding_type));
                ImGui::Text("Value Type: %s", INPUT_VALUE_TYPE_STR(ipa.value_type));

                if (ImGui::CollapsingHeader("Keybinds"))
                { 
                    ImGui::Indent();
                    if (ipa.input_source == INPUT_SOURCE::KEYBOARD)
                    {
                        if (ipa.binding_type == INPUT_BINDING_TYPE::AXIS_2D)
                        {
                            ipa.input_keys[0].keyboard_key = KeyboardKeyChange("UP", ipa.input_keys[0].keyboard_key);
                            ipa.input_keys[1].keyboard_key = KeyboardKeyChange("DOWN", ipa.input_keys[1].keyboard_key);
                            ipa.input_keys[2].keyboard_key = KeyboardKeyChange("RIGHT", ipa.input_keys[2].keyboard_key);
                            ipa.input_keys[3].keyboard_key = KeyboardKeyChange("LEFT", ipa.input_keys[3].keyboard_key);
                        }
                        else if (ipa.binding_type == INPUT_BINDING_TYPE::AXIS_1D)
                        {
                            ipa.input_keys[0].keyboard_key = KeyboardKeyChange("UP", ipa.input_keys[0].keyboard_key);
                            ipa.input_keys[1].keyboard_key = KeyboardKeyChange("DOWN", ipa.input_keys[1].keyboard_key);
                        }
                        else
                        {
                            ipa.input_keys[0].keyboard_key = KeyboardKeyChange("Key", ipa.input_keys[0].keyboard_key);
                        }
                    }
                    if (ipa.input_source == INPUT_SOURCE::MOUSE)
                    {
                        ImGui::Text("Mouse input: %s", MOUSE_INPUT_STR(ipa.input_keys[0].mouse_input));
                    }
                    ImGui::Unindent();
                }
                ImGui::Unindent();
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        ImGui::Unindent();
    }
}

void Editor::ImGuiDisplayGames()
{
    if (ImGui::CollapsingHeader("game instances"))
    {
        if (ImGui::Button("load game"))
        {
            PathString search_path{};

            if (OSFindDirectoryNameDialogWindow(search_path.data(), search_path.capacity(), L"Game project directory search"))
            {
                search_path.RecalculateStringSize();
                if (OSDirectoryExist(search_path.c_str()))
                {
                    const size_t dir_slash_pos = search_path.find_last_of_directory_slash();
                    const StringView dir_name = search_path.GetView(search_path.size() - dir_slash_pos - 1, dir_slash_pos + 1);
                    AddGameInstance(dir_name, ConstSlice<PFN_LuaPluginRegisterFunctions>());
                }
            }
        }
        ImGui::Indent();
        for (uint32_t i = 0; i < m_game_instances.size(); i++)
        {
            ImGui::PushID(static_cast<int>(i));
            EditorGame& game = m_game_instances[i];
            if (ImGui::CollapsingHeader(game.GetGameInstance().m_project_name.GetView().c_str()))
            {
                if (ImGui::Button("unload"))
                {
                    game.Destroy();
                    m_game_instances.Erase(i);
                }
            }
            ImGui::PopID();
        }
        ImGui::Unindent();
    }
}
