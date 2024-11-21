#pragma once
#include "ViewportInterface.hpp"
#include "Camera.hpp"

namespace BB
{

	static_assert(is_interactable_viewport_interface<RenderViewport>);
	class RenderViewport
	{
	public:
		bool Init(const uint2 a_game_viewport_size, const uint32_t a_back_buffer_count);
		bool Update(const float a_delta_time);
		bool HandleInput(const float a_delta_time, const Slice<InputEvent> a_input_events);
		// maybe ifdef this for editor
		void DisplayImGuiInfo();
		void Destroy();

		Viewport& GetViewport() { return m_viewport; }
		SceneHierarchy& GetSceneHierarchy() { return m_scene_hierarchy; }

	private:
		Viewport m_viewport;
		SceneHierarchy m_scene_hierarchy;

		FreeCamera camera{};
		float speed = 0.25f;
		float min_speed = 0.1f;
		float max_speed = 1.0f;
		FreeCameraOption m_free_cam;
	};
}
