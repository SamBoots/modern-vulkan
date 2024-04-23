#pragma once
#include "Common.h"
#include "SceneHierarchy.hpp"
#include "Camera.hpp"
#include "AssetLoader.hpp"
#include "GameInterface.hpp"

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
			a_game_interface.Update(a_arena, a_input_events);
			StaticArray<SceneHierarchy>& scene_hierarchies = a_game_interface.GetSceneHierarchies();
			(void)scene_hierarchies;
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
		static void LoadAssetsAsync(void* a_params);

		class Viewport
		{
		public:
			void Init(MemoryArena& a_arena, const uint2 a_extent, const uint2 a_offset, const char* a_name);
			void Resize(const uint2 a_new_extent);

			void DrawScene(const RCommandList a_list, const SceneHierarchy& a_scene_hierarchy);
			void DrawImgui(bool& a_resized, const uint2 a_minimum_size = uint2(160, 80));

			bool PositionWithinViewport(const uint2 a_pos) const;

			float4x4 CreateProjection(const float a_fov, const float a_near_field, const float a_far_field) const;
			float4x4 CreateView() const;

			const char* GetName() const { return m_name; }
			Camera& GetCamera() { return m_camera; }

		private:
			uint2 m_extent;
			uint2 m_offset; // offset into main window NOT USED NOW 
			RenderTarget m_render_target;
			const char* m_name;
			Camera m_camera{ float3{0.0f, 0.0f, 1.0f}, 0.35f };
		};
		void MainEditorImGuiInfo(const MemoryArena& a_arena);
		struct ThreadFuncForDrawing_Params
		{
			Viewport& viewport;
			SceneHierarchy& scene_hierarchy;
			RCommandList command_list;
		};
		static void ThreadFuncForDrawing(void* a_param);

		StaticArray<MaterialInfo> m_materials;
		StaticArray<ShaderEffectInfo> m_shader_effects;

		struct ViewportAndScene
		{
			Viewport viewport;
			SceneHierarchy scene;
		};
		uint2 m_window_extent;
		StaticArray<ViewportAndScene> m_viewport_and_scenes;

		// temp
		ShaderEffectHandle m_imgui_vertex;
		ShaderEffectHandle m_imgui_fragment;

		Viewport* m_active_viewport = nullptr;
		float2 m_previous_mouse_pos{};

		GPUDeviceInfo m_gpu_info;

		WindowHandle m_main_window;

		bool m_freeze_cam = false;
		float m_cam_speed = 1.f;
		const float m_cam_speed_min = 0.1f;
		const float m_cam_speed_max = 3.f;
	};
}
