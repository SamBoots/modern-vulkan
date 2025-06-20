#pragma once
#include "ViewportInterface.hpp"
#include "SceneHierarchy.hpp"
#include "lua/LuaEngine.hpp"

namespace BB
{
	class RenderViewport
	{
	public:
		bool Init(const uint2 a_game_viewport_size, const uint32_t a_back_buffer_count, const StringView a_project_name);
		bool Update(const float a_delta_time, const bool a_selected);
		void Destroy();
        
		float3 GetCameraPos();
        float4x4 GetCameraView();

		Viewport& GetViewport() { return m_viewport; }
        InputChannelHandle GetInputChannel() const { return m_input_channel; }
        SceneHierarchy& GetSceneHierarchy() { return m_scene_hierarchy; }

	private:
		MemoryArena m_memory;
		Viewport m_viewport;
		SceneHierarchy m_scene_hierarchy;
		LuaECSEngine m_context;
        InputChannelHandle m_input_channel;
        PathString m_project_path;
	};
	static_assert(is_interactable_viewport_interface<RenderViewport>);
}
