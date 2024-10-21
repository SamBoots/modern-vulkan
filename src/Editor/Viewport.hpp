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
		void Init(const uint2 a_extent, const int2 a_offset, const uint32_t a_render_target_count, const StringView a_name);
		void Resize(const uint2 a_new_extent);
		void DrawImgui(bool& a_resized, uint32_t a_back_buffer_index, const uint2 a_minimum_size = uint2(160, 80));

		const RImageView StartRenderTarget(const RCommandList a_cmd_list, uint32_t a_back_buffer_index) const;
		void EndRenderTarget(const RCommandList a_cmd_list, const uint32_t a_back_buffer_index, const IMAGE_LAYOUT a_current_layout);

		bool PositionWithinViewport(const uint2 a_pos) const;

		float4x4 CreateProjection(const float a_fov, const float a_near_field, const float a_far_field) const;

		const StringView GetName() const { return m_name; }
		const uint2 GetExtent() const { return m_extent; }
		const int2 GetOffset() const { return m_offset; }

	private:
		void CreateTextures();

		uint2 m_extent;
		int2 m_offset;
		uint32_t m_render_target_count;
		RImage m_image;
		RDescriptorIndex m_image_indices[3];
		StringView m_name;
	};
}
