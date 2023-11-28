#include "imgui.h"      // IMGUI_IMPL_API
#include "Renderer.hpp"
#include "HID.h"

namespace BB
{
	struct ImGui_ImplBB_InitInfo
	{
		WindowHandle window;
		uint32_t image_count = 0;
		uint32_t min_image_count = 0;
	};

	// Called by user code
	IMGUI_IMPL_API bool ImGui_ImplBB_Init(Allocator a_temp_allocator, const RCommandList a_cmd_list, const ImGui_ImplBB_InitInfo& a_info, UploadBufferView& a_upload_view);
	IMGUI_IMPL_API void ImGui_ImplBB_Shutdown();
	IMGUI_IMPL_API void ImGui_ImplBB_NewFrame();
	IMGUI_IMPL_API void ImGui_ImplBB_RenderDrawData(const ImDrawData& a_draw_data, const RCommandList a_command_list);

	IMGUI_IMPL_API bool ImGui_ImplBB_ProcessInput(const InputEvent& a_input_event);
}
