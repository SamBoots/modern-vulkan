#include "Viewport.hpp"
#include "SceneHierarchy.hpp"
#include "Renderer.hpp"
#include "Program.h"
#include "Math.inl"

#include "imgui.h"

using namespace BB;

void Viewport::Init(MemoryArena& a_arena, const uint2 a_extent, const uint2 a_offset, const StringView a_name)
{
	m_extent = a_extent;
	m_offset = a_offset;
	m_render_target = CreateRenderTarget(a_arena, a_extent, a_name.c_str());
	// use constructors and destructors :)))))))))))))))))))))))))))
	new (&m_name)StringView(a_name);
}

void Viewport::Resize(const uint2 a_new_extent)
{
	if (m_extent == a_new_extent)
		return;

	m_extent = a_new_extent;
	ResizeRenderTarget(m_render_target, a_new_extent);
}

void Viewport::DrawScene(const RCommandList a_list, const SceneHierarchy& a_scene_hierarchy)
{
	StartRenderTarget(a_list, m_render_target);

	a_scene_hierarchy.DrawSceneHierarchy(a_list, m_render_target, m_extent, int2(0, 0));

	EndRenderTarget(a_list, m_render_target);
}

void Viewport::DrawImgui(bool& a_resized, const uint2 a_minimum_size)
{
	a_resized = false;
	if (ImGui::Begin(m_name.c_str(), nullptr, ImGuiWindowFlags_MenuBar))
	{
		const RTexture render_target = GetCurrentRenderTargetTexture(m_render_target);
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("screenshot"))
			{
				static char image_name[128]{};
				ImGui::InputText("sceenshot name", image_name, 128);

				if (ImGui::Button("make screenshot"))
				{
					// reimplement this to use the new asset system
					BB_UNIMPLEMENTED();
					// just hard stall, this is a button anyway

					//CommandPool& pool = GetGraphicsCommandPool();
					//const RCommandList list = pool.StartCommandList();

					//GPUBufferCreateInfo readback_info;
					//readback_info.name = "viewport screenshot readback";
					//readback_info.size = static_cast<uint64_t>(m_extent.x * m_extent.y * 4u);
					//readback_info.type = BUFFER_TYPE::READBACK;
					//readback_info.host_writable = true;
					//GPUBuffer readback = CreateGPUBuffer(readback_info);

					//ReadTexture(render_target, m_extent, int2(0, 0), readback, readback_info.size);

					//pool.EndCommandList(list);
					//uint64_t fence;
					//BB_ASSERT(ExecuteGraphicCommands(Slice(&pool, 1), fence), "Failed to make a screenshot");

					//StackString<256> image_name_bmp{ "screenshots" };
					//if (OSDirectoryExist(image_name_bmp.c_str()))
					//	OSCreateDirectory(image_name_bmp.c_str());

					//image_name_bmp.push_back('/');
					//image_name_bmp.append(image_name);
					//image_name_bmp.append(".png");

					//// maybe deadlock if it's never idle...
					//GPUWaitIdle();

					//const void* readback_mem = MapGPUBuffer(readback);

					//BB_ASSERT(Asset::WriteImage(image_name_bmp.c_str(), m_extent.x, m_extent.y, 4, readback_mem), "failed to write screenshot image to disk");

					//UnmapGPUBuffer(readback);
					//FreeGPUBuffer(readback);
				}

				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}

		ImGuiIO im_io = ImGui::GetIO();

		const ImVec2 window_offset = ImGui::GetWindowPos();
		m_offset = uint2(static_cast<unsigned int>(window_offset.x), static_cast<unsigned int>(window_offset.y));

		if (static_cast<unsigned int>(ImGui::GetWindowSize().x) < a_minimum_size.x ||
			static_cast<unsigned int>(ImGui::GetWindowSize().y) < a_minimum_size.y)
		{
			ImGui::SetWindowSize(ImVec2(static_cast<float>(a_minimum_size.x), static_cast<float>(a_minimum_size.y)));
			ImGui::End();
			return;
		}

		const ImVec2 viewport_draw_area = ImGui::GetContentRegionAvail();

		const uint2 window_size_u = uint2(static_cast<unsigned int>(viewport_draw_area.x), static_cast<unsigned int>(viewport_draw_area.y));
		if (window_size_u != m_extent && !im_io.WantCaptureMouse)
		{
			a_resized = true;
			Resize(window_size_u);
		}

		ImGui::Image(render_target.handle, viewport_draw_area);

	}
	ImGui::End();
}

bool Viewport::PositionWithinViewport(const uint2 a_pos) const
{
	if (m_offset.x < a_pos.x &&
		m_offset.y < a_pos.y &&
		m_offset.x + m_extent.x > a_pos.x &&
		m_offset.y + m_extent.y > a_pos.y)
		return true;
	return false;
}

float4x4 Viewport::CreateProjection(const float a_fov, const float a_near_field, const float a_far_field) const
{
	return Float4x4Perspective(ToRadians(a_fov), static_cast<float>(m_extent.x) / static_cast<float>(m_extent.y), a_near_field, a_far_field);
}
