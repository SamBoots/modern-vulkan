#pragma once

#include <concepts>
#include "SceneHierarchy.hpp"
#include "HID.h"

namespace BB
{
	struct ViewportRect
	{
		uint2 size;
		uint2 offset;
	};

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

		const uint2 GetExtent() const { return m_extent; }
		const int2 GetOffset() const { return m_offset; }

	private:
		void Screenshot(const uint32_t a_back_buffer_index, const char* a_name) const;
		void CreateTextures();

		StringView m_name;
		uint2 m_extent;
		int2 m_offset;
		uint32_t m_render_target_count;
		RImage m_image;
		RDescriptorIndex m_image_indices[3];
	};

	template <typename T>
	concept is_interactable_viewport_interface = requires(T v, const float a_delta_time, Slice<InputEvent> a_input_events)
	{
		{ v.Update(a_delta_time) } -> std::same_as<bool>;
		{ v.HandleInput(a_delta_time, a_input_events) } -> std::same_as<bool>;
		// maybe ifdef this for editor
		v.DisplayImGuiInfo();

		{ v.GetViewport() } -> std::same_as<BB::Viewport&>;
		{ v.GetSceneHierarchy() } -> std::same_as<BB::SceneHierarchy&>;
	};
}
