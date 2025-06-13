#pragma once
#include "ViewportInterface.hpp"
#include "Camera.hpp"
#include "SceneHierarchy.hpp"

namespace BB
{
	class RenderViewport
	{
	public:
		bool Init(const uint2 a_game_viewport_size, const uint32_t a_back_buffer_count, const StringView a_json_path);
		bool Update(const float a_delta_time, const bool a_selected);
		// maybe ifdef this for editor
        void DisplayImGuiInfo();
		void Destroy();
        
        float3 GetCameraPos() const { return m_camera.GetPosition(); }

		Viewport& GetViewport() { return m_viewport; }
        SceneHierarchy& GetSceneHierarchy() { return m_scene_hierarchy; }

	private:
		MemoryArena m_memory;
		Viewport m_viewport;
		SceneHierarchy m_scene_hierarchy;

		FreeCamera m_camera{};
		float m_speed = 1.f;
		float m_min_speed = 0.1f;
		float m_max_speed = 100.0f;

        // input
        InputActionHandle m_move_forward_backward_left_right;
        InputActionHandle m_move_speed_slider;
        InputActionHandle m_look_around;
        InputActionHandle m_enable_rotate_button;
	};
	static_assert(is_interactable_viewport_interface<RenderViewport>);
}
