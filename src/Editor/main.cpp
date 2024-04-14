//entry for the vulkan renderer
#include "BBMain.h"
#include "BBMemory.h"
#include "Program.h"
#include "BBThreadScheduler.hpp"
#include "HID.h"

#include <chrono>

#include "shared_common.hlsl.h"

#include "Math.inl"

#include "Editor.hpp"

using namespace BB;

static void CustomCloseWindow(const BB::WindowHandle a_window_handle)
{
	(void)a_window_handle;
	BB_ASSERT(false, "unimplemented");
}

static void CustomResizeWindow(const BB::WindowHandle a_window_handle, const uint32_t a_x, const uint32_t a_y)
{
	(void)a_x;
	(void)a_y;
	(void)a_window_handle;
	BB::RequestResize();
}

int main(int argc, char** argv)
{
	(void)argc;

	StackString<512> exe_path;

	{
		const StringView exe_path_manipulator{ argv[0] };
		const size_t path_end = exe_path_manipulator.find_last_of('\\');

		exe_path.append(exe_path_manipulator.c_str(), path_end);
	}
	
	BBInitInfo bb_init{};
	bb_init.exe_path = exe_path.c_str();
	bb_init.program_name = L"Modern Vulkan - Editor";
	InitBB(bb_init);

	SystemInfo sys_info;
	OSSystemInfo(sys_info);
	Threads::InitThreads(sys_info.processor_num / 2);

	MemoryArena main_arena = MemoryArenaCreate();

	const uint2 window_extent = uint2(1280, 720);

	const WindowHandle window_handle = CreateOSWindow(
		BB::OS_WINDOW_STYLE::MAIN,
		static_cast<int>(window_extent.x) / 4,
		static_cast<int>(window_extent.y) / 4,
		static_cast<int>(window_extent.x),
		static_cast<int>(window_extent.y),
		L"Modern Vulkan - editor");

	RendererCreateInfo render_create_info;
	render_create_info.app_name = "modern vulkan - editor";
	render_create_info.engine_name = "building block engine - editor";
	render_create_info.window_handle = window_handle;
	render_create_info.swapchain_width = window_extent.x;
	render_create_info.swapchain_height = window_extent.y;
	render_create_info.debug = true;

	InitializeRenderer(main_arena, render_create_info);

	{
		const Asset::AssetManagerInitInfo asset_manager_info = {};
		Asset::InitializeAssetManager(asset_manager_info);
	}

	Editor editor{};
	editor.Init(main_arena, window_handle, window_extent);

	MemoryArenaScope(main_arena)
	{
		ShaderEffectHandle shader_effects[3]{};

		CreateShaderEffectInfo shader_effect_create_infos[3];
		shader_effect_create_infos[0].name = "debug vertex shader";
		shader_effect_create_infos[0].stage = SHADER_STAGE::VERTEX;
		shader_effect_create_infos[0].next_stages = static_cast<uint32_t>(SHADER_STAGE::FRAGMENT_PIXEL);
		shader_effect_create_infos[0].shader_path = "../../resources/shaders/hlsl/Debug.hlsl";
		shader_effect_create_infos[0].shader_entry = "VertexMain";
		shader_effect_create_infos[0].push_constant_space = sizeof(ShaderIndices);
		shader_effect_create_infos[0].pass_type = RENDER_PASS_TYPE::STANDARD_3D;

		shader_effect_create_infos[1].name = "debug fragment shader";
		shader_effect_create_infos[1].stage = SHADER_STAGE::FRAGMENT_PIXEL;
		shader_effect_create_infos[1].next_stages = static_cast<uint32_t>(SHADER_STAGE::NONE);
		shader_effect_create_infos[1].shader_path = "../../resources/shaders/hlsl/Debug.hlsl";
		shader_effect_create_infos[1].shader_entry = "FragmentMain";
		shader_effect_create_infos[1].push_constant_space = sizeof(ShaderIndices);
		shader_effect_create_infos[1].pass_type = RENDER_PASS_TYPE::STANDARD_3D;

		shader_effect_create_infos[2].name = "jitter vertex shader";
		shader_effect_create_infos[2].stage = SHADER_STAGE::VERTEX;
		shader_effect_create_infos[2].next_stages = static_cast<uint32_t>(SHADER_STAGE::FRAGMENT_PIXEL);
		shader_effect_create_infos[2].shader_path = "../../resources/shaders/hlsl/Jitter.hlsl";
		shader_effect_create_infos[2].shader_entry = "VertexMain";
		shader_effect_create_infos[2].push_constant_space = sizeof(ShaderIndices);
		shader_effect_create_infos[2].pass_type = RENDER_PASS_TYPE::STANDARD_3D;

		BB_ASSERT(editor.CreateShaderEffect(main_arena,
			Slice(shader_effect_create_infos, _countof(shader_effect_create_infos)),
			shader_effects), "Failed to create shader objects");

		CreateMaterialInfo material_info;
		material_info.name = "default material";
		material_info.shader_effects = Slice<const ShaderEffectHandle>(shader_effects, 2);

		const MaterialHandle base_material = editor.CreateMaterial(material_info);
		SetDefaultMaterial(base_material);
	}

	editor.CreateViewportViaJson(main_arena, "../../resources/scenes/standard_scene.json", "game scene", window_extent, float3(0.1f, 0.1f, 0.5f));
	editor.CreateViewportViaJson(main_arena, "../../resources/scenes/object_scene.json", "object viewer", window_extent / uint2(2), float3(0.1f, 0.1f, 0.5f));

	SetWindowCloseEvent(CustomCloseWindow);
	SetWindowResizeEvent(CustomResizeWindow);

	auto current_time = std::chrono::high_resolution_clock::now();

	bool quit_app = false;
	float delta_time = 0;
	while (!quit_app)
	{
		Asset::Update();
		
		editor.Update(main_arena, delta_time);
		auto currentnew = std::chrono::high_resolution_clock::now();
		delta_time = std::chrono::duration<float, std::chrono::seconds::period>(currentnew - current_time).count();


		current_time = currentnew;
	}

	editor.Destroy();

	return 0;
}
