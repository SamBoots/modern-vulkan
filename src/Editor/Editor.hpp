#pragma once
#include "Common.h"
#include "SceneHierarchy.hpp"
#include "Camera.hpp"
#include "AssetLoader.hpp"
#include "GameInterface.hpp"
#include "Viewport.hpp"

#include <tuple>

namespace BB
{
	constexpr size_t EDITOR_DEFAULT_MEMORY = gbSize * 4;

	struct MemoryArena;
	class Editor
	{
	public:
		void Init(MemoryArena& a_arena, const WindowHandle a_window, const uint2 a_window_extent, const size_t a_editor_memory = EDITOR_DEFAULT_MEMORY);
		void CreateViewportViaJson(MemoryArena& a_arena, const char* a_json_path, const char* a_viewport_name, const uint2 a_window_extent, const float3 a_clear_color);
		void Destroy();

		template<typename game_interface>
		requires is_game_interface<game_interface>
		void Update(MemoryArena& a_arena, const float a_delta_time, game_interface& a_game_interface, const Slice<InputEvent> a_input_events)
		{
			a_game_interface.Update(a_input_events);
			SceneHierarchy& hierarchy = a_game_interface.GetSceneHierarchy();
			(void)hierarchy;


			Update(a_arena, a_delta_time, a_input_events);
		}

		bool CreateShaderEffect(MemoryArena& a_temp_arena, const Slice<CreateShaderEffectInfo> a_create_infos, ShaderEffectHandle* const a_handles);
		const MaterialHandle CreateMaterial(const CreateMaterialInfo& a_create_info);

		static ThreadTask LoadAssets(const Slice<Asset::AsyncAsset> a_asyn_assets, const char* a_cmd_list_name = "upload asset task");

		struct ShaderEffectInfo
		{
			ShaderEffectHandle handle;
			StringView name;
			StringView entry_point;
			SHADER_STAGE shader_stage;
			SHADER_STAGE_FLAGS next_stages;
			Buffer shader_data;
		};

		struct MaterialInfo
		{
			MaterialHandle handle;
			StringView name;
			size_t shader_handle_count;
			ShaderEffectHandle* shader_handles;
		};

	private:
		void Update(MemoryArena& a_arena, const float a_delta_time, const Slice<InputEvent> a_input_events);
		FreelistInterface m_editor_allocator;

		void ImguiDisplaySceneHierarchy(SceneHierarchy& a_hierarchy);
		void ImGuiDisplaySceneObject(SceneHierarchy& a_hierarchy, const SceneObjectHandle a_object);
		void ImGuiDisplayShaderEffect(const ShaderEffectHandle a_handle) const;
		void ImGuiDisplayShaderEffects();
		void ImGuiDisplayMaterial(const MaterialHandle a_handle) const;
		void ImGuiDisplayMaterials();
		void ImGuiCreateMaterial();

		struct LoadAssetsAsync_params
		{
			Editor* editor;
			MemoryArena arena;
			Asset::AsyncAsset* assets;
			size_t asset_count;
			const char* cmd_list_name = "upload asset task";
		};
		static void LoadAssetsAsync(MemoryArena& a_thread_arena, void* a_params);
		void MainEditorImGuiInfo(const MemoryArena& a_arena);
		struct ThreadFuncForDrawing_Params
		{
			Viewport& viewport;
			SceneHierarchy& scene_hierarchy;
			RCommandList command_list;
		};
		static void ThreadFuncForDrawing(MemoryArena& a_thread_arena, void* a_param);

		StaticArray<MaterialInfo> m_materials;
		StaticArray<ShaderEffectInfo> m_shader_effects;

		struct ViewportAndScene
		{
			Viewport viewport;
			FreeCamera camera{ float3{0.0f, 0.0f, 1.0f}, 0.35f };
			SceneHierarchy scene;
		};
		uint2 m_app_window_extent;
		StaticArray<ViewportAndScene> m_viewport_and_scenes;

		// temp
		ShaderEffectHandle m_imgui_vertex;
		ShaderEffectHandle m_imgui_fragment;

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
