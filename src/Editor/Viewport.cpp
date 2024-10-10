#include "Viewport.hpp"
#include "SceneHierarchy.hpp"
#include "Renderer.hpp"
#include "Program.h"
#include "Math.inl"

#include "imgui.h"

using namespace BB;

void Viewport::Init(MemoryArena& a_arena, const uint2 a_extent, const int2 a_offset, const uint32_t a_render_target_count, const StringView a_name)
{
	m_extent = a_extent;
	m_offset = a_offset;
	m_textures.Init(a_arena, a_render_target_count);
	m_name = a_name;
	CreateTextures();
}

void Viewport::Resize(const uint2 a_new_extent)
{
	if (m_extent == a_new_extent)
		return;

	m_extent = a_new_extent;
	for (uint32_t i = 0; i < m_textures.size(); i++)
	{
		FreeTexture(m_textures[i]);
	}
	CreateTextures();
}

void Viewport::DrawImgui(bool& a_resized, uint64_t a_back_buffer_index, const uint2 a_minimum_size)
{
	a_resized = false;
	if (ImGui::Begin(m_name.c_str(), nullptr, ImGuiWindowFlags_MenuBar))
	{
		const RTexture render_target = m_textures[a_back_buffer_index];
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
		m_offset = int2(static_cast<int>(window_offset.x), static_cast<int>(window_offset.y));

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

const RTexture& Viewport::StartRenderTarget(const RCommandList a_cmd_list, uint64_t a_back_buffer_index) const
{
	const RTexture& render_target = m_textures[a_back_buffer_index];

	PipelineBarrierImageInfo render_target_transition;
	render_target_transition.src_mask = BARRIER_ACCESS_MASK::NONE;
	render_target_transition.dst_mask = BARRIER_ACCESS_MASK::COLOR_ATTACHMENT_WRITE;
	render_target_transition.image = GetImage(render_target);
	render_target_transition.old_layout = IMAGE_LAYOUT::UNDEFINED;
	render_target_transition.new_layout = IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL;
	render_target_transition.layer_count = 1;
	render_target_transition.level_count = 1;
	render_target_transition.base_array_layer = 0;
	render_target_transition.base_mip_level = 0;
	render_target_transition.src_stage = BARRIER_PIPELINE_STAGE::TOP_OF_PIPELINE;
	render_target_transition.dst_stage = BARRIER_PIPELINE_STAGE::COLOR_ATTACH_OUTPUT;

	PipelineBarrierInfo pipeline_info{};
	pipeline_info.image_info_count = 1;
	pipeline_info.image_infos = &render_target_transition;
	PipelineBarriers(a_cmd_list, pipeline_info);

	return render_target;
}

void Viewport::EndRenderTarget(const RCommandList a_cmd_list, const RTexture& a_render_target, const IMAGE_LAYOUT a_current_layout)
{
	PipelineBarrierImageInfo render_target_transition;
	render_target_transition.src_mask = BARRIER_ACCESS_MASK::COLOR_ATTACHMENT_WRITE;
	render_target_transition.dst_mask = BARRIER_ACCESS_MASK::SHADER_READ;
	render_target_transition.image = GetImage(a_render_target);
	render_target_transition.old_layout = a_current_layout;
	render_target_transition.new_layout = IMAGE_LAYOUT::SHADER_READ_ONLY;
	render_target_transition.layer_count = 1;
	render_target_transition.level_count = 1;
	render_target_transition.base_array_layer = 0;
	render_target_transition.base_mip_level = 0;
	render_target_transition.src_stage = BARRIER_PIPELINE_STAGE::COLOR_ATTACH_OUTPUT;
	render_target_transition.dst_stage = BARRIER_PIPELINE_STAGE::FRAGMENT_SHADER;

	PipelineBarrierInfo pipeline_info{};
	pipeline_info.image_info_count = 1;
	pipeline_info.image_infos = &render_target_transition;
	PipelineBarriers(a_cmd_list, pipeline_info);
}

bool Viewport::PositionWithinViewport(const uint2 a_pos) const
{
	if (m_offset.x < static_cast<int>(a_pos.x) &&
		m_offset.y < static_cast<int>(a_pos.y) &&
		m_offset.x + static_cast<int>(m_extent.x) > static_cast<int>(a_pos.x) &&
		m_offset.y + static_cast<int>(m_extent.y) > static_cast<int>(a_pos.y))
		return true;
	return false;
}

float4x4 Viewport::CreateProjection(const float a_fov, const float a_near_field, const float a_far_field) const
{
	return Float4x4Perspective(ToRadians(a_fov), static_cast<float>(m_extent.x) / static_cast<float>(m_extent.y), a_near_field, a_far_field);
}

void  Viewport::CreateTextures()
{
	for (size_t i = 0; i < m_textures.size(); i++)
	{
		CreateTextureInfo render_target_info;
		render_target_info.width = m_extent.x;
		render_target_info.height = m_extent.y;
		render_target_info.format = RENDER_TARGET_IMAGE_FORMAT;
		render_target_info.usage = IMAGE_USAGE::RENDER_TARGET;
		render_target_info.array_layers = 1; // TODO: make this equal to m_texture_count and remove the dynamic array
		render_target_info.name = m_name.c_str();
		m_textures[i] = CreateTexture(render_target_info);
	}
}
