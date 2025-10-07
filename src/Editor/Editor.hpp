#pragma once
#include "Common.h"
#include "SceneHierarchy.hpp"
#include "ViewportInterface.hpp"
#include "MaterialSystem.hpp"
#include "BBThreadScheduler.hpp"
#include "Console.hpp"
#include "Gizmo.hpp"
#include "HID.h"
#include "EditorGame.hpp"

#include <tuple>

namespace BB
{
	constexpr size_t EDITOR_DEFAULT_MEMORY = mbSize * 4;

    struct EditorGameCreateInfo
    {
        StringView dir_name;
        ConstSlice<PFN_LuaPluginRegisterFunctions> register_funcs;
    };

    struct EditorCreateInfo
    {
        WindowHandle window;
        uint2 window_extent;
        uint32_t game_instance_max;
        ConstSlice<EditorGameCreateInfo> initial_games;
    };

	struct MemoryArena;
	class Editor
	{
	public:
		void Init(MemoryArena& a_arena, const EditorCreateInfo& a_create_info);
		void Destroy();

		void StartFrame(MemoryArena& a_arena, const Slice<InputEvent> a_input_events, const float a_delta_time);
        void UpdateGames(MemoryArena& a_arena, const float a_delta_time);

        void AddGameInstance(const StringView a_dir_path, const ConstSlice<PFN_LuaPluginRegisterFunctions> a_register_funcs);
        ThreadTask UpdateGameInstance(const float a_delta_time, class EditorGame& a_game);
		void EndFrame(MemoryArena& a_arena);

		bool ResizeWindow(const uint2 a_window);

	private:
		struct ThreadFuncForDrawing_Params
		{
			Editor& editor;
            float delta_time;
            class EditorGame& instance;
		};

		bool DrawImgui(const RDescriptorIndex a_render_target, Viewport& a_viewport);

        void ImGuiDisplayEditor(MemoryArena& a_arena);
        void ImGuiDisplayGame(class EditorGame& a_game);
		void ImguiDisplayECS(EntityComponentSystem& a_ecs, const uint2 a_viewport_extent);
		void ImGuiDisplayEntity(EntityComponentSystem& a_ecs, const ECSEntity a_object, const uint2 a_viewport_extent);
		void ImGuiCreateEntity(EntityComponentSystem& a_ecs, const ECSEntity a_parent = INVALID_ECS_OBJ);
		void ImGuiDisplayShaderEffect(MemoryArenaTemp a_temp_arena, const CachedShaderInfo& a_shader_infom, int& a_reload_status) const;
		void ImGuiDisplayShaderEffects(MemoryArena& a_arena);
		void ImGuiDisplayMaterial(const MasterMaterial& a_material) const;
		void ImGuiDisplayMaterials();
        void ImGuiDisplayInputChannel(const InputChannelHandle a_channel);
        void ImGuiDisplayGames();

		void MainEditorImGuiInfo(const MemoryArena& a_arena) const;
		static void ThreadFuncForDrawing(MemoryArena& a_thread_arena, void* a_param);
		void UpdateGame(class EditorGame& a_instance, const float a_delta_time);
        void UpdateGizmo(Viewport& a_viewport, SceneHierarchy& a_hierarchy, const float3 a_cam_pos);

		uint2 m_app_window_extent;
		Console m_console;

		RImage m_render_target;
		FixedArray<RDescriptorIndex, 3> m_render_target_descs;
        StaticArray<EditorGame> m_game_instances;

        struct DrawStruct
        {
            class EditorGame* game = nullptr;
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
            InputActionHandle toggle_editor_cam;

            InputActionHandle camera_move;
            InputActionHandle move_speed_slider;
            InputActionHandle look_around;
            InputActionHandle enable_rotate;
        } m_input;
	};
}
