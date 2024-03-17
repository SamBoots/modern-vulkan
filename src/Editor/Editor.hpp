#pragma once
#include "Common.h"
#include "SceneHierarchy.hpp"

namespace BB
{
	struct Viewport
	{
		uint2 extent;
		uint2 offset; // offset into main window NOT USED NOW 
		RenderTarget render_target;
		const char* name;
		Camera camera{ float3{0.0f, 0.0f, 1.0f}, 0.35f };
	};

	struct MemoryArena;
	class Editor
	{
	public:
		void Init(MemoryArena& a_arena, const uint2 window_extent);
		void Update(MemoryArena& a_arena, const float a_delta_time);

	private:
		Viewport m_game_screen;
		Viewport m_object_viewer_screen;

		SceneHierarchy m_game_hierarchy;
		SceneHierarchy m_object_viewer_hierarchy;

		Viewport* m_active_viewport = nullptr;
		float2 previous_mouse_pos{};

		bool freeze_cam = false;
	}
}