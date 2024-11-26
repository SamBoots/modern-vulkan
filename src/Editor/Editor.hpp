#pragma once
#include "Common.h"
#include "SceneHierarchy.hpp"
#include "Camera.hpp"
#include "AssetLoader.hpp"
#include "ViewportInterface.hpp"
#include "MaterialSystem.hpp"

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

		void StartFrame(const Slice<InputEvent> a_input_events, const float a_delta_time);

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

			ImguiDisplaySceneHierarchy(hierarchy);

			bool resized = false;
			viewport.DrawImgui(resized, m_per_frame.back_buffer_index);
			if (resized)
			{
				hierarchy.SetProjection(viewport.CreateProjection(60.f, 0.001f, 10000.0f));
			}

			const uint32_t render_pool_index = m_per_frame.current_count.fetch_add(1, std::memory_order_seq_cst);
			ThreadFuncForDrawing_Params& params = m_per_frame.params[render_pool_index];
			CommandPool& pool = m_per_frame.pools[render_pool_index];
			RCommandList& list = m_per_frame.lists[render_pool_index];
			hierarchy.IncrementNextFenceValue(&m_per_frame.fences[render_pool_index], &m_per_frame.fence_values[render_pool_index]);
			// render_pool 0 is already set.
			if (render_pool_index != 0)
			{
				pool = GetGraphicsCommandPool();
				list = pool.StartCommandList();
			}

			params.back_buffer_index = m_per_frame.back_buffer_index;
			params.viewport = &viewport;
			params.scene_hierarchy = &hierarchy;
			params.command_list = list;

			return Threads::StartTaskThread(ThreadFuncForDrawing, &params, L"scene draw task");
		}
		void EndFrame(MemoryArena& a_arena);

	private:
		FreelistInterface m_editor_allocator;

		void ImguiDisplaySceneHierarchy(SceneHierarchy& a_hierarchy);
		void ImGuiDisplaySceneObject(SceneHierarchy& a_hierarchy, const SceneObjectHandle a_object);
		void ImguiCreateSceneObject(SceneHierarchy& a_hierarchy, const SceneObjectHandle a_parent = INVALID_SCENE_OBJ);
		void ImGuiDisplayShaderEffect(MemoryArena& a_temp_arena, const CachedShaderInfo& a_shader_info) const;
		void ImGuiDisplayShaderEffects(MemoryArena& a_temp_arena);
		void ImGuiDisplayMaterial(const MasterMaterial& a_material) const;
		void ImGuiDisplayMaterials();

		void MainEditorImGuiInfo(const MemoryArena& a_arena);
		struct ThreadFuncForDrawing_Params
		{
			uint32_t back_buffer_index;
			Viewport* viewport;
			SceneHierarchy* scene_hierarchy;
			RCommandList command_list;
		};
		static void ThreadFuncForDrawing(MemoryArena& a_thread_arena, void* a_param);

		uint2 m_app_window_extent;

		struct PerFrameInfo
		{
			uint32_t back_buffer_index;
			FixedArray<ThreadFuncForDrawing_Params, 8> params;
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
