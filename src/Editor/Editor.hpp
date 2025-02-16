#pragma once
#include "Common.h"
#include "SceneHierarchy.hpp"
#include "Camera.hpp"
#include "ViewportInterface.hpp"
#include "MaterialSystem.hpp"
#include "BBThreadScheduler.hpp"
#include "Console.hpp"

#include <tuple>

namespace BB
{
	constexpr size_t EDITOR_DEFAULT_MEMORY = mbSize * 4;

	struct MemoryArena;
	class Editor
	{
	public:
		void Init(MemoryArena& a_arena, const WindowHandle a_window, const uint2 a_window_extent, const size_t a_editor_memory = EDITOR_DEFAULT_MEMORY);
		void Destroy();

		void StartFrame(MemoryArena& a_arena, const Slice<InputEvent> a_input_events, const float a_delta_time);

		template<typename viewport_interface>
		requires is_interactable_viewport_interface<viewport_interface>
		ThreadTask UpdateViewport(MemoryArena& a_arena, const float a_delta_time, viewport_interface& a_game_interface, const Slice<InputEvent> a_input_events)
		{
			(void)a_arena;
			Viewport& viewport = a_game_interface.GetViewport();
			SceneHierarchy& hierarchy = a_game_interface.GetSceneHierarchy();

			if (!m_swallow_input && viewport.PositionWithinViewport(uint2(static_cast<unsigned int>(m_previous_mouse_pos.x), static_cast<unsigned int>(m_previous_mouse_pos.y))))
			{
				a_game_interface.HandleInput(a_delta_time, a_input_events);
			}

			a_game_interface.Update(a_delta_time);

			ImguiDisplayECS(hierarchy.m_ecs);

			const uint32_t render_pool_index = m_per_frame.current_count.fetch_add(1, std::memory_order_seq_cst);
			ThreadFuncForDrawing_Params params;
			CommandPool& pool = m_per_frame.pools[render_pool_index];
			RCommandList& list = m_per_frame.lists[render_pool_index];
			// render_pool 0 is already set.
			if (render_pool_index != 0)
			{
				pool = GetGraphicsCommandPool();
				list = pool.StartCommandList();
			}

			params.viewport = &viewport;
			params.scene_hierarchy = &hierarchy;
			params.command_list = list;
			params.fence_value = &m_per_frame.fence_values[render_pool_index];
			params.fence = &m_per_frame.fences[render_pool_index];
			params.scene_frame = &m_per_frame.frame_results[render_pool_index];

			scene_hierarchy.DrawImgui(param_in->scene_frame->render_frame.render_target, viewport);

			return Threads::StartTaskThread(ThreadFuncForDrawing, &params, sizeof(params), L"scene draw task");
		}
		void EndFrame(MemoryArena& a_arena);

	private:
		struct ThreadFuncForDrawing_Params
		{
			// in
			Viewport* viewport;
			SceneHierarchy* scene_hierarchy;
			RCommandList command_list;
			uint64_t* fence_value;
			RFence* fence;
			SceneFrame* scene_frame;
		};

		bool DrawImgui(const RDescriptorIndex a_render_target, SceneHierarchy& a_hierarchy, Viewport& a_viewport)
		{
			bool rendered_image = false;
			if (ImGui::Begin(a_hierarchy.GetECS().GetName().c_str(), nullptr, ImGuiWindowFlags_MenuBar))
			{
				if (ImGui::BeginMenuBar())
				{
					if (ImGui::BeginMenu("screenshot"))
					{
						static char image_name[128]{};
						ImGui::InputText("sceenshot name", image_name, 128);

						if (ImGui::Button("make screenshot"))
							a_hierarchy.GetECS().GetRenderSystem().Screenshot(image_name);

						ImGui::EndMenu();
					}
					ImGui::EndMenuBar();
				}

				ImGuiIO im_io = ImGui::GetIO();

				constexpr uint2 MINIMUM_WINDOW_SIZE = uint2(80, 80);

				const ImVec2 viewport_offset = ImGui::GetWindowPos();
				const ImVec2 viewport_draw_area = ImGui::GetContentRegionAvail();
				const uint2 window_size_u = uint2(static_cast<unsigned int>(viewport_draw_area.x), static_cast<unsigned int>(viewport_draw_area.y));
				if (window_size_u.x < MINIMUM_WINDOW_SIZE.x || window_size_u.y < MINIMUM_WINDOW_SIZE.y)
				{
					ImGui::End();
					return false;
				}
				if (window_size_u != a_viewport.GetExtent() && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
				{
					a_viewport.SetExtent(window_size_u);
				}
				a_viewport.SetOffset(int2(static_cast<int>(viewport_offset.x), static_cast<int>(viewport_offset.y)));

				ImGui::Image(a_render_target.handle, viewport_draw_area);
				rendered_image = true;
			}
			ImGui::End();

			return rendered_image;
		}

		FreelistInterface m_editor_allocator;

		void ImguiDisplayECS(EntityComponentSystem& a_ecs);
		void ImGuiDisplayEntity(EntityComponentSystem& a_ecs, const ECSEntity a_object);
		void ImguiCreateEntity(EntityComponentSystem& a_ecs, const ECSEntity a_parent = INVALID_ECS_OBJ);
		void ImGuiDisplayShaderEffect(MemoryArenaTemp a_temp_arena, const CachedShaderInfo& a_shader_infom, int& a_reload_status) const;
		void ImGuiDisplayShaderEffects(MemoryArena& a_arena);
		void ImGuiDisplayMaterial(const MasterMaterial& a_material) const;
		void ImGuiDisplayMaterials();

		void MainEditorImGuiInfo(const MemoryArena& a_arena);
		static void ThreadFuncForDrawing(MemoryArena& a_thread_arena, void* a_param);

		uint2 m_app_window_extent;
		Console m_console;

		struct PerFrameInfo
		{
			uint32_t back_buffer_index;
			FixedArray<SceneFrame, 8> frame_results;
			FixedArray<CommandPool, 8> pools;
			FixedArray<RCommandList, 8> lists;
			FixedArray<RFence, 8> fences;
			FixedArray<uint64_t, 8> fence_values;

			std::atomic<uint32_t> current_count = 0;
		};
		PerFrameInfo m_per_frame;

		MasterMaterialHandle m_imgui_material;

		// input info
		bool m_swallow_input;
		float2 m_previous_mouse_pos;

		GPUDeviceInfo m_gpu_info;

		WindowHandle m_main_window;
	};
}
