#include "Editor.hpp"

#include "Program.h"
#include "MemoryArena.hpp"
#include "math.inl"

#include "Renderer.hpp"
#include "imgui.h"

#include "Camera.hpp"
#include "SceneHierarchy.hpp"

using namespace BB;
using namespace Editor;

struct Viewport
{
	uint2 extent;
	uint2 offset; // offset into main window NOT USED NOW 
	RenderTarget render_target;
	const char* name;
	Camera camera{ float3{0.0f, 0.0f, 1.0f}, 0.35f };
};

struct Editor_inst
{
	Viewport game_screen;
	Viewport object_viewer_screen;

	SceneHierarchy game_hierarchy;
	SceneHierarchy object_viewer_hierarchy;
};

struct Editor_inst* s_editor;

void Editor::Init(struct BB::MemoryArena& a_arena, const uint2 window_extent)
{
	s_editor = ArenaAllocType(a_arena, Editor_inst);

	s_editor->game_screen = CreateViewport(a_arena, window_extent, uint2(), "game scene");
	s_editor->object_viewer_screen = CreateViewport(a_arena, window_extent / 2u, uint2(), "object viewer");

	s_editor->game_hierarchy.InitializeSceneHierarchy(a_arena, 128, "normal scene");
	s_editor->object_viewer_hierarchy.InitializeSceneHierarchy(a_arena, 16, "object viewer scene");
	s_editor->game_hierarchy.SetClearColor(float3{ 0.1f, 0.6f, 0.1f });
	s_editor->object_viewer_hierarchy.SetClearColor(float3{ 0.5f, 0.1f, 0.1f });
}

void Editor::Update()
{

}

static Viewport CreateViewport(MemoryArena& a_arena, const uint2 a_extent, const uint2 a_offset, const char* a_name)
{
	Viewport viewport{};
	viewport.extent = a_extent;
	viewport.offset = a_offset;
	viewport.render_target = CreateRenderTarget(a_arena, a_extent, a_name);
	viewport.name = a_name;
	return viewport;
}

static void ViewportResize(Viewport& a_viewport, const uint2 a_new_extent)
{
	if (a_viewport.extent == a_new_extent)
		return;

	a_viewport.extent = a_new_extent;
	ResizeRenderTarget(a_viewport.render_target, a_new_extent);
}

static void MainDebugWindow(const MemoryArena& a_arena, const Viewport* a_selected_viewport)
{
	if (ImGui::Begin("general engine info"))
	{
		if (a_selected_viewport == nullptr)
			ImGui::Text("Current viewport: None");
		else
			ImGui::Text("Current viewport: %s", a_selected_viewport->name);

		if (ImGui::CollapsingHeader("main allocator"))
		{
			ImGui::Indent();
			ImGui::Text("standard reserved memory: %zu", ARENA_DEFAULT_RESERVE);
			ImGui::Text("commit size %zu", ARENA_DEFAULT_COMMIT);

			if (ImGui::CollapsingHeader("main stack memory arena"))
			{
				ImGui::Indent();
				const size_t remaining = MemoryArenaSizeRemaining(a_arena);
				const size_t commited = MemoryArenaSizeCommited(a_arena);
				const size_t used = MemoryArenaSizeUsed(a_arena);

				ImGui::Text("memory remaining: %zu", remaining);
				ImGui::Text("memory commited: %zu", commited);
				ImGui::Text("memory used: %zu", used);

				const float perc_calculation = 1.f / static_cast<float>(ARENA_DEFAULT_COMMIT);
				ImGui::Separator();
				ImGui::TextUnformatted("memory used till next commit");
				ImGui::ProgressBar(perc_calculation * static_cast<float>(RoundUp(used, ARENA_DEFAULT_COMMIT) - used));
				ImGui::Unindent();
			}
		}
	}
	ImGui::End();
}

static void DrawImGuiViewport(Viewport& a_viewport, bool& a_resized, const uint2 a_minimum_size = uint2(160, 80))
{
	a_resized = false;
	if (ImGui::Begin(a_viewport.name, nullptr, ImGuiWindowFlags_MenuBar))
	{
		const RTexture render_target = GetCurrentRenderTargetTexture(a_viewport.render_target);
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("screenshot"))
			{
				static char image_name[128]{};
				ImGui::InputText("sceenshot name", image_name, 128);

				if (ImGui::Button("make screenshot"))
				{
					// just hard stall, this is a button anyway

					CommandPool& pool = GetGraphicsCommandPool();
					const RCommandList list = pool.StartCommandList();

					GPUBufferCreateInfo readback_info;
					readback_info.name = "viewport screenshot readback";
					readback_info.size = static_cast<uint64_t>(a_viewport.extent.x * a_viewport.extent.y * 4u);
					readback_info.type = BUFFER_TYPE::READBACK;
					readback_info.host_writable = true;
					GPUBuffer readback = CreateGPUBuffer(readback_info);

					ReadTexture(list, render_target, a_viewport.extent, int2(0, 0), readback, readback_info.size);

					pool.EndCommandList(list);
					uint64_t fence;
					BB_ASSERT(ExecuteGraphicCommands(Slice(&pool, 1), fence), "Failed to make a screenshot");

					StackString<256> image_name_bmp{ "screenshots" };
					if (OSDirectoryExist(image_name_bmp.c_str()))
						OSCreateDirectory(image_name_bmp.c_str());

					image_name_bmp.push_back('/');
					image_name_bmp.append(image_name);
					image_name_bmp.append(".png");

					// maybe deadlock if it's never idle...
					GPUWaitIdle();

					const void* readback_mem = MapGPUBuffer(readback);

					BB_ASSERT(Asset::WriteImage(image_name_bmp.c_str(), a_viewport.extent.x, a_viewport.extent.y, 4, readback_mem), "failed to write screenshot image to disk");

					UnmapGPUBuffer(readback);
					FreeGPUBuffer(readback);
				}

				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}


		ImGuiIO im_io = ImGui::GetIO();

		const ImVec2 window_offset = ImGui::GetWindowPos();
		a_viewport.offset = uint2(static_cast<unsigned int>(window_offset.x), static_cast<unsigned int>(window_offset.y));

		if (static_cast<unsigned int>(ImGui::GetWindowSize().x) < a_minimum_size.x ||
			static_cast<unsigned int>(ImGui::GetWindowSize().y) < a_minimum_size.y)
		{
			ImGui::SetWindowSize(ImVec2(static_cast<float>(a_minimum_size.x), static_cast<float>(a_minimum_size.y)));
			ImGui::End();
			return;
		}

		const ImVec2 viewport_draw_area = ImGui::GetContentRegionAvail();

		const uint2 window_size_u = uint2(static_cast<unsigned int>(viewport_draw_area.x), static_cast<unsigned int>(viewport_draw_area.y));
		if (window_size_u != a_viewport.extent && !im_io.WantCaptureMouse)
		{
			a_resized = true;
			ViewportResize(a_viewport, window_size_u);
		}

		ImGui::Image(render_target.handle, viewport_draw_area);
	}
	ImGui::End();
}

static bool PositionWithinViewport(const Viewport& a_viewport, const uint2 a_pos)
{
	if (a_viewport.offset.x < a_pos.x &&
		a_viewport.offset.y < a_pos.y &&
		a_viewport.offset.x + a_viewport.extent.x > a_pos.x &&
		a_viewport.offset.y + a_viewport.extent.y > a_pos.y)
		return true;
	return false;
}