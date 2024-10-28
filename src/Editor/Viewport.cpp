#include "Viewport.hpp"
#include "SceneHierarchy.hpp"
#include "Renderer.hpp"
#include "Program.h"
#include "Math.inl"

#include "imgui.h"

using namespace BB;

void Viewport::Init(const uint2 a_extent, const int2 a_offset, const uint32_t a_render_target_count, const StringView a_name)
{
	BB_ASSERT(_countof(m_image_indices) >= a_render_target_count, "viewport not supporting more then 3 render targets");
	m_extent = a_extent;
	m_offset = a_offset;
	m_render_target_count = a_render_target_count;
	m_name = a_name;
	CreateTextures();
}

void Viewport::Resize(const uint2 a_new_extent)
{
	if (m_extent == a_new_extent)
		return;

	m_extent = a_new_extent;
	FreeImage(m_image);
	for (uint32_t i = 0; i < m_render_target_count; i++)
	{
		FreeImageView(m_image_indices[i]);
	}
	CreateTextures();
}

void Viewport::DrawImgui(bool& a_resized, const uint32_t a_back_buffer_index, const uint2 a_minimum_size)
{
	a_resized = false;
	if (ImGui::Begin(m_name.c_str(), nullptr, ImGuiWindowFlags_MenuBar))
	{
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("screenshot"))
			{
				static char image_name[128]{};
				ImGui::InputText("sceenshot name", image_name, 128);

				if (ImGui::Button("make screenshot"))
				{
					GPUWaitIdle();

					CommandPool& pool = GetGraphicsCommandPool();
					const RCommandList list = pool.StartCommandList();

					{
						PipelineBarrierImageInfo read_image;
						read_image.image = m_image;
						read_image.old_layout = IMAGE_LAYOUT::SHADER_READ_ONLY;
						read_image.new_layout = IMAGE_LAYOUT::TRANSFER_SRC;
						read_image.aspects = IMAGE_ASPECT::COLOR;
						read_image.base_array_layer = a_back_buffer_index;
						read_image.base_mip_level = 0;
						read_image.layer_count = 1;
						read_image.level_count = 1;
						read_image.src_stage = BARRIER_PIPELINE_STAGE::FRAGMENT_SHADER;
						read_image.dst_stage = BARRIER_PIPELINE_STAGE::TRANSFER;
						read_image.src_mask = BARRIER_ACCESS_MASK::SHADER_READ;
						read_image.dst_mask = BARRIER_ACCESS_MASK::TRANSFER_READ;
						read_image.src_queue = QUEUE_TRANSITION::NO_TRANSITION;
						read_image.dst_queue = QUEUE_TRANSITION::NO_TRANSITION;

						PipelineBarrierInfo read_info{};
						read_info.image_infos = &read_image;
						read_info.image_info_count = 1;
						PipelineBarriers(list, read_info);
					}

					GPUBufferCreateInfo readback_info;
					readback_info.name = "viewport screenshot readback";
					readback_info.size = static_cast<uint64_t>(m_extent.x * m_extent.y * 4u);
					readback_info.type = BUFFER_TYPE::READBACK;
					readback_info.host_writable = true;
					GPUBuffer readback = CreateGPUBuffer(readback_info);

					// this is deffered so it works later.
					BB_UNIMPLEMENTED();
					ReadTexture(m_image, IMAGE_LAYOUT::TRANSFER_SRC, m_extent, int2(0, 0), readback, readback_info.size);

					{
						PipelineBarrierImageInfo read_image;
						read_image.image = m_image;
						read_image.old_layout = IMAGE_LAYOUT::TRANSFER_SRC;
						read_image.new_layout = IMAGE_LAYOUT::SHADER_READ_ONLY;
						read_image.aspects = IMAGE_ASPECT::COLOR;
						read_image.base_array_layer = a_back_buffer_index;
						read_image.base_mip_level = 0;
						read_image.layer_count = 1;
						read_image.level_count = 1;
						read_image.src_stage = BARRIER_PIPELINE_STAGE::TRANSFER;
						read_image.dst_stage = BARRIER_PIPELINE_STAGE::FRAGMENT_SHADER;
						read_image.src_mask = BARRIER_ACCESS_MASK::TRANSFER_READ;
						read_image.dst_mask = BARRIER_ACCESS_MASK::SHADER_READ;
						read_image.src_queue = QUEUE_TRANSITION::NO_TRANSITION;
						read_image.dst_queue = QUEUE_TRANSITION::NO_TRANSITION;

						PipelineBarrierInfo read_info{};
						read_info.image_infos = &read_image;
						read_info.image_info_count = 1;
						PipelineBarriers(list, read_info);
					}

					pool.EndCommandList(list);
					uint64_t fence;
					ExecuteGraphicCommands(Slice(&pool, 1), nullptr, nullptr, 0, fence);

					StackString<256> image_name_bmp{ "screenshots" };
					// if (OSDirectoryExist(image_name_bmp.c_str()))
					OSCreateDirectory(image_name_bmp.c_str());

					image_name_bmp.push_back('/');
					image_name_bmp.append(image_name);
					image_name_bmp.append(".png");

					// maybe deadlock if it's never idle...
					GPUWaitIdle();

					const void* readback_mem = MapGPUBuffer(readback);

					BB_ASSERT(Asset::WriteImage(image_name_bmp.c_str(), m_extent.x, m_extent.y, 4, readback_mem), "failed to write screenshot image to disk");

					UnmapGPUBuffer(readback);
					FreeGPUBuffer(readback);
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

		ImGui::Image(m_image_indices[a_back_buffer_index].handle, viewport_draw_area);
	}
	ImGui::End();
}

const RImageView Viewport::StartRenderTarget(const RCommandList a_cmd_list, uint32_t a_back_buffer_index) const
{
	BB_ASSERT(a_back_buffer_index <= m_render_target_count, "back buffer index accessing render target out of bounds");
	PipelineBarrierImageInfo render_target_transition;
	render_target_transition.src_mask = BARRIER_ACCESS_MASK::NONE;
	render_target_transition.dst_mask = BARRIER_ACCESS_MASK::COLOR_ATTACHMENT_WRITE;
	render_target_transition.image = m_image;
	render_target_transition.old_layout = IMAGE_LAYOUT::UNDEFINED;
	render_target_transition.new_layout = IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL;
	render_target_transition.layer_count = 1;
	render_target_transition.level_count = 1;
	render_target_transition.base_array_layer = a_back_buffer_index;
	render_target_transition.base_mip_level = 0;
	render_target_transition.src_stage = BARRIER_PIPELINE_STAGE::TOP_OF_PIPELINE;
	render_target_transition.dst_stage = BARRIER_PIPELINE_STAGE::COLOR_ATTACH_OUTPUT;
	render_target_transition.aspects = IMAGE_ASPECT::COLOR;

	PipelineBarrierInfo pipeline_info{};
	pipeline_info.image_info_count = 1;
	pipeline_info.image_infos = &render_target_transition;
	PipelineBarriers(a_cmd_list, pipeline_info);

	return GetImageView(m_image_indices[a_back_buffer_index]);
}

void Viewport::EndRenderTarget(const RCommandList a_cmd_list, const uint32_t a_back_buffer_index, const IMAGE_LAYOUT a_current_layout)
{
	PipelineBarrierImageInfo render_target_transition;
	render_target_transition.src_mask = BARRIER_ACCESS_MASK::COLOR_ATTACHMENT_WRITE;
	render_target_transition.dst_mask = BARRIER_ACCESS_MASK::SHADER_READ;
	render_target_transition.image = m_image;
	render_target_transition.old_layout = a_current_layout;
	render_target_transition.new_layout = IMAGE_LAYOUT::SHADER_READ_ONLY;
	render_target_transition.layer_count = 1;
	render_target_transition.level_count = 1;
	render_target_transition.base_array_layer = a_back_buffer_index;
	render_target_transition.base_mip_level = 0;
	render_target_transition.src_stage = BARRIER_PIPELINE_STAGE::COLOR_ATTACH_OUTPUT;
	render_target_transition.dst_stage = BARRIER_PIPELINE_STAGE::FRAGMENT_SHADER;
	render_target_transition.aspects = IMAGE_ASPECT::COLOR;

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
	ImageCreateInfo image_create;
	image_create.name = m_name.c_str();
	image_create.width = m_extent.x;
	image_create.height = m_extent.y;
	image_create.depth = 1;
	image_create.array_layers = static_cast<uint16_t>(m_render_target_count);
	image_create.mip_levels = 1;
	image_create.type = IMAGE_TYPE::TYPE_2D;
	image_create.format = RENDER_TARGET_IMAGE_FORMAT;
	image_create.usage = IMAGE_USAGE::RENDER_TARGET;
	image_create.use_optimal_tiling = true;
	image_create.is_cube_map = false;
	m_image = CreateImage(image_create);

	ImageViewCreateInfo image_view_create;
	image_view_create.name = m_name.c_str();
	image_view_create.array_layers = 1;
	image_view_create.mip_levels = 1;
	image_view_create.base_mip_level = 0;
	image_view_create.image = m_image;
	image_view_create.format = RENDER_TARGET_IMAGE_FORMAT;
	image_view_create.type = IMAGE_VIEW_TYPE::TYPE_2D;
	image_view_create.aspects = IMAGE_ASPECT::COLOR;

	for (uint32_t i = 0; i < m_render_target_count; i++)
	{
		image_view_create.base_array_layer = static_cast<uint16_t>(i);
		m_image_indices[i] = CreateImageView(image_view_create);
	}
}
