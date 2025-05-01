#pragma once
#include "Enginefwd.hpp"
#include "Rendererfwd.hpp"
#include "HID.h"

namespace BB
{
	bool ImInit(MemoryArena& a_arena, const WindowHandle a_window_handle);
	void ImShutdown();
	
	void ImNewFrame(const uint2 a_screen_extent);

	void ImRenderFrame(const RCommandList a_cmd_list, const RImageView a_render_target_view, const uint2 a_render_target_extent, const bool a_clear_image, const ShaderEffectHandle a_vertex, const ShaderEffectHandle a_fragment);
	//On true means that imgui takes the input and doesn't give it to the engine.
	bool ImProcessInput(const struct BB::InputEvent& a_input_event);
}
