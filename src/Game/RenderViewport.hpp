#pragma once
#include "ViewportInterface.hpp"
#include "Camera.hpp"
#include "SceneHierarchy.hpp"
#include "Gizmo.hpp"

namespace BB
{
	class RenderViewport
	{
	public:
		bool Init(const uint2 a_game_viewport_size, const uint32_t a_back_buffer_count, const StringView a_json_path);
		bool Update(const float a_delta_time);
		bool HandleInput(const float a_delta_time, const Slice<InputEvent> a_input_events);
		// maybe ifdef this for editor
        void DisplayImGuiInfo();
		void Destroy();

		Viewport& GetViewport() { return m_viewport; }
		SceneHierarchy& GetSceneHierarchy() { return m_scene_hierarchy; }

	private:
		MemoryArena m_memory;
		Viewport m_viewport;
		SceneHierarchy m_scene_hierarchy;
        ECSEntity m_selected_entity;
        Gizmo m_gizmo;
        GIZMO_HIT_FLAGS m_gizmo_hits;

		FreeCamera m_camera{};
        bool m_rotate_enable;
		float m_speed = 200.f;
		float m_min_speed = 10.f;
		float m_max_speed = 1000.0f;
	};
	static_assert(is_interactable_viewport_interface<RenderViewport>);
}
