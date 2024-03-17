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
using namespace Editor;

static Viewport CreateViewport(MemoryArena& a_arena, const uint2 a_extent, const uint2 a_offset, const char* a_name)
{
	Viewport viewport{};
	viewport.extent = a_extent;
	viewport.offset = a_offset;
	viewport.render_target = CreateRenderTarget(a_arena, a_extent, a_name);
	viewport.name = a_name;
	return viewport;
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

void Editor::Init(struct BB::MemoryArena& a_arena, const uint2 window_extent)
{
	s_editor = ArenaAllocType(a_arena, Editor_inst);

	m_game_screen = CreateViewport(a_arena, window_extent, uint2(), "game scene");
	m_object_viewer_screen = CreateViewport(a_arena, window_extent / 2u, uint2(), "object viewer");

	m_game_hierarchy.InitializeSceneHierarchy(a_arena, 128, "normal scene");
	m_object_viewer_hierarchy.InitializeSceneHierarchy(a_arena, 16, "object viewer scene");
	m_game_hierarchy.SetClearColor(float3{ 0.1f, 0.6f, 0.1f });
	m_object_viewer_hierarchy.SetClearColor(float3{ 0.5f, 0.1f, 0.1f });
}

void Editor::Update(MemoryArena& a_arena, const float a_delta_time)
{
	InputEvent input_events[INPUT_EVENT_BUFFER_MAX]{};
	size_t input_event_count = 0;

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
			if (!freeze_cam && active_viewport)
			{
				active_viewport->camera.Move(cam_move);
			}

		}
		else if (ip.input_type == INPUT_TYPE::MOUSE)
		{
			const MouseInfo& mi = ip.mouse_info;
			const float2 mouse_move = (mi.move_offset * delta_time) * 10.f;
			m_previous_mouse_pos = mi.mouse_pos;

			if (mi.right_released)
				FreezeMouseOnWindow(window);
			if (mi.left_released)
				UnfreezeMouseOnWindow();

			if (PositionWithinViewport(viewport_scene, uint2(static_cast<unsigned int>(mi.mouse_pos.x), static_cast<unsigned int>(mi.mouse_pos.y))))
			{
				m_active_viewport = &m_viewport_scene;
			}
			else if (PositionWithinViewport(viewport_object_viewer, uint2(static_cast<unsigned int>(mi.mouse_pos.x), static_cast<unsigned int>(mi.mouse_pos.y))))
			{
				m_active_viewport = &m_viewport_object_viewer;
			}

			if (!freeze_cam && active_viewport)
			{
				active_viewport->camera.Rotate(mouse_move.x, mouse_move.y);
			}
		}
	}

	CommandPool graphics_command_pools[2]{ GetGraphicsCommandPool(), GetGraphicsCommandPool() };
	const RCommandList main_list = graphics_command_pools[0].StartCommandList();

	StartFrameInfo start_info;
	start_info.delta_time = delta_time;
	start_info.mouse_pos = previous_mouse_pos;

	StartFrame(main_list, start_info);

	MemoryArenaScope(main_arena)
	{

		m_scene_hierarchy.SetView(m_viewport_scene.camera.CalculateView());
		m_object_viewer_scene.SetView(m_viewport_object_viewer.camera.CalculateView());

		Asset::ShowAssetMenu(main_arena);
		MainDebugWindow(main_arena, active_viewport);
		m_scene_hierarchy.ImguiDisplaySceneHierarchy();
		m_object_viewer_scene.ImguiDisplaySceneHierarchy();

		bool resized = false;
		DrawImGuiViewport(m_viewport_scene, resized);
		if (resized)
		{
			m_scene_hierarchy.SetProjection(CalculateProjection(float2(static_cast<float>(m_viewport_scene.extent.x), static_cast<float>(m_viewport_scene.extent.y))));
		}

		resized = false;
		DrawImGuiViewport(m_viewport_object_viewer, resized);
		if (resized)
		{
			m_object_viewer_scene.SetProjection(CalculateProjection(float2(static_cast<float>(m_viewport_object_viewer.extent.x), static_cast<float>(m_viewport_object_viewer.extent.y))));
		}

		ThreadFuncForDrawing_Params main_scene_params{
			m_viewport_scene,
			m_scene_hierarchy,
			main_list
		};

		const RCommandList object_viewer_list = graphics_command_pools[1].StartCommandList();

		ThreadFuncForDrawing_Params object_viewer_params{
			m_viewport_object_viewer,
			m_object_viewer_scene,
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
