#pragma once
#include "MemoryArena.hpp"
#include "Common.h"
#include "Rendererfwd.hpp"
#include "Camera.hpp"
#include "Storage/BBString.h"

namespace BB
{
	class Viewport
	{
	public:
		void Init(MemoryArena& a_arena, const uint2 a_extent, const uint2 a_offset, const StringView a_name);
		void Resize(const uint2 a_new_extent);

		void DrawScene(const RCommandList a_list, const class SceneHierarchy& a_scene_hierarchy);
		void DrawImgui(bool& a_resized, const uint2 a_minimum_size = uint2(160, 80));

		bool PositionWithinViewport(const uint2 a_pos) const;

		float4x4 CreateProjection(const float a_fov, const float a_near_field, const float a_far_field) const;

		const StringView GetName() const { return m_name; }

	private:
		uint2 m_extent;
		uint2 m_offset; // offset into main window NOT USED NOW 
		RenderTarget m_render_target;
		StringView m_name;
	};
}
