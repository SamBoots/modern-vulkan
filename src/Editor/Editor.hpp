#pragma once
#include "Common.h"
#include "SceneHierarchy.hpp"
#include "ViewportInterface.hpp"
#include "MaterialSystem.hpp"
#include "BBThreadScheduler.hpp"
#include "Console.hpp"
#include "Gizmo.hpp"
#include "HID.h"

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

        ThreadTask UpdateGameInstance(MemoryArena& a_arena, const float a_delta_time, class GameInstance& a_game);
		void EndFrame(MemoryArena& a_arena);

		bool ResizeWindow(const uint2 a_window);

	private:
		struct ThreadFuncForDrawing_Params
		{
			Editor& editor;
            float delta_time;
            class GameInstance& instance;
		};

		bool DrawImgui(const RDescriptorIndex a_render_target, SceneHierarchy& a_hierarchy, Viewport& a_viewport);

		FreelistInterface m_editor_allocator;

		void ImguiDisplayECS(EntityComponentSystem& a_ecs);
		void ImGuiDisplayEntity(EntityComponentSystem& a_ecs, const ECSEntity a_object);
		void ImguiCreateEntity(EntityComponentSystem& a_ecs, const ECSEntity a_parent = INVALID_ECS_OBJ);
		void ImGuiDisplayShaderEffect(MemoryArenaTemp a_temp_arena, const CachedShaderInfo& a_shader_infom, int& a_reload_status) const;
		void ImGuiDisplayShaderEffects(MemoryArena& a_arena);
		void ImGuiDisplayMaterial(const MasterMaterial& a_material) const;
		void ImGuiDisplayMaterials();
        void ImGuiDisplayInputChannel(const InputChannelHandle a_channel);

		void MainEditorImGuiInfo(const MemoryArena& a_arena);
		static void ThreadFuncForDrawing(MemoryArena& a_thread_arena, void* a_param);
		void UpdateGame(class GameInstance& a_instance, const float a_delta_time);
        void UpdateGizmo(Viewport& a_viewport, SceneHierarchy& a_hierarchy, const float3 a_cam_pos);

		uint2 m_app_window_extent;
		Console m_console;

		RImage m_render_target;
		FixedArray<RDescriptorIndex, 3> m_render_target_descs;

        enum class DRAW_TYPE
        {
            GAME
        };

        struct DrawStruct
        {
            DRAW_TYPE type;
            union
            {
                class GameInstance* game = nullptr;
            };
        };

		struct PerFrameInfo
		{
			FixedArray<CommandPool, 8> pools;
			FixedArray<RCommandList, 8> lists;
			FixedArray<DrawStruct, 8> draw_struct;
			FixedArray<RFence, 8> fences;
			FixedArray<uint64_t, 8> fence_values;
			FixedArray<SceneFrame, 8> frame_results;
            FixedArray<bool, 8> success;
            FixedArray<StackString<64>, 8> error_message;

			std::atomic<uint32_t> current_count = 0;
			uint32_t back_buffer_index;
		};
		PerFrameInfo m_per_frame;

		MasterMaterialHandle m_imgui_material;

		// input info
		bool m_swallow_input;
		float2 m_previous_mouse_pos;

		GPUDeviceInfo m_gpu_info;

		WindowHandle m_main_window;

        Gizmo m_gizmo;

        struct Controls
        {
            InputChannelHandle channel;
            InputActionHandle click_on_screen;
            InputActionHandle mouse_move;
            InputActionHandle gizmo_toggle_scale;
        } m_input;
	};
}
