#pragma once
#include "Common.h"
#include "SceneHierarchy.hpp"
#include "Camera.hpp"
#include "AssetLoader.hpp"
#include "GameInterface.hpp"
#include "Viewport.hpp"
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
		void CreateSceneHierarchyViaJson(MemoryArena& a_arena, SceneHierarchy& a_hierarchy, const uint32_t a_back_buffer_count, const char* a_json_path);
		void CreateSceneHierarchyViaJson(MemoryArena& a_arena, SceneHierarchy& a_hierarchy, const uint32_t a_back_buffer_count, const JsonParser& a_parsed_file);
		void RegisterSceneHierarchy(SceneHierarchy& a_hierarchy, const uint2 a_window_extent, const uint32_t a_back_buffer_count);

		template<typename game_interface>
		requires is_game_interface<game_interface>
		void Update(MemoryArena& a_arena, const float a_delta_time, game_interface& a_game_interface, const Slice<InputEvent> a_input_events)
		{
			a_game_interface.Update(a_input_events);
			SceneHierarchy& hierarchy = a_game_interface.GetSceneHierarchy();
			(void)hierarchy;

			Update(a_arena, a_delta_time, a_input_events);
		}

		static ThreadTask LoadAssets(const Slice<Asset::AsyncAsset> a_asyn_assets, Editor* a_editor);

	private:
		void Update(MemoryArena& a_arena, const float a_delta_time, const Slice<InputEvent> a_input_events);
		FreelistInterface m_editor_allocator;

		void ImguiDisplaySceneHierarchy(SceneHierarchy& a_hierarchy);
		void ImGuiDisplaySceneObject(SceneHierarchy& a_hierarchy, const SceneObjectHandle a_object);
		void ImguiCreateSceneObject(SceneHierarchy& a_hierarchy, const SceneObjectHandle a_parent = INVALID_SCENE_OBJ);
		void ImGuiDisplayShaderEffect(MemoryArena& a_temp_arena, const CachedShaderInfo& a_shader_info) const;
		void ImGuiDisplayShaderEffects(MemoryArena& a_temp_arena);
		void ImGuiDisplayMaterial(const MasterMaterial& a_material) const;
		void ImGuiDisplayMaterials();

		struct LoadAssetsAsync_params
		{
			Editor* editor;
			MemoryArena arena;
			Asset::AsyncAsset* assets;
			size_t asset_count;
		};
		static void LoadAssetsAsync(MemoryArena& a_thread_arena, void* a_params);
		void MainEditorImGuiInfo(const MemoryArena& a_arena);
		struct ThreadFuncForDrawing_Params
		{
			uint32_t back_buffer_index;
			Viewport& viewport;
			SceneHierarchy& scene_hierarchy;
			RCommandList command_list;
		};
		static void ThreadFuncForDrawing(MemoryArena& a_thread_arena, void* a_param);

		StaticArray<StringView> m_loaded_models_names;

		struct ViewportAndScene
		{
			SceneHierarchy& scene;
			Viewport viewport{};
			FreeCamera camera{ float3{0.0f, 0.0f, 1.0f}, 0.35f };
		};
		uint2 m_app_window_extent;
		StaticArray<ViewportAndScene> m_viewport_and_scenes;

		MasterMaterialHandle m_imgui_material;

		ViewportAndScene* m_active_viewport = nullptr;
		float2 m_previous_mouse_pos{};

		GPUDeviceInfo m_gpu_info;

		WindowHandle m_main_window;

		bool m_freeze_cam = false;
		float m_cam_speed = 1.f;
		const float m_cam_speed_min = 0.1f;
		const float m_cam_speed_max = 3.f;
	};
}
