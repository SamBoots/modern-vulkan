#pragma once
#include "Common.h"
#include "SceneHierarchy.hpp"
#include "Camera.hpp"
#include "AssetLoader.hpp"

namespace BB
{
	struct MemoryArena;
	class Editor
	{
	public:
		void Init(MemoryArena& a_arena, const FixedArray<ShaderEffectHandle, 2>& a_TEMP_shader_effects, const WindowHandle a_window, const uint2 a_window_extent);
		void Destroy();
		void Update(MemoryArena& a_arena, const float a_delta_time);

		static ThreadTask LoadAssets(const Slice<Asset::AsyncAsset> a_asyn_assets, const char* a_cmd_list_name = "upload asset task");

	private:
		struct LoadAssetsAsync_params
		{
			Editor* editor;
			Slice<Asset::AsyncAsset> asyn_assets;
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

		StaticArray<MaterialHandle> m_materials;
		StaticArray<ShaderEffectHandle> m_shader_effects;

		Viewport m_game_screen;
		Viewport m_object_viewer_screen;

		SceneHierarchy m_game_hierarchy;
		SceneHierarchy m_object_viewer_hierarchy;

		Viewport* m_active_viewport = nullptr;
		float2 m_previous_mouse_pos{};

		GPUDeviceInfo m_gpu_info;

		WindowHandle m_main_window;

		bool m_freeze_cam = false;
	};
}
