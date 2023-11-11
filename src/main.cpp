//entry for the vulkan renderer
#include "BBMain.h"
#include "BBMemory.h"
#include "Program.h"
#include "BBThreadScheduler.hpp"
#include "HID.h"

#include "Camera.hpp"
#include "Transform.hpp"

#include "AssetLoader.hpp"

#include <chrono>

#include "Renderer.hpp"
#include "Math.inl"

static void DrawglTFNode(BB::Model::Node& a_node, const BB::float4x4& a_transform)
{
	BB::float4x4 local_transform = a_transform * a_node.transform;

	if (a_node.mesh_handle.handle != BB::BB_INVALID_HANDLE)
		BB::DrawMesh(a_node.mesh_handle, local_transform);
	for (size_t i = 0; i < a_node.child_count; i++)
	{
		DrawglTFNode(a_node.childeren[i], local_transform);
	}
}

int main(int argc, char** argv)
{
	(void)argc;
	using namespace BB;

	BBInitInfo bb_init;
	bb_init.exePath = argv[0];
	bb_init.programName = L"Modern Vulkan";
	InitBB(bb_init);

	SystemInfo sys_info;
	OSSystemInfo(sys_info);
	Threads::InitThreads(sys_info.processor_num);

	StackAllocator_t main_allocator{ mbSize * 32 };
	TransformPool transform_pool{ main_allocator, 32 };
	const TransformHandle transform_test = transform_pool.CreateTransform(float3{ 0, -1, 1 });
	const TransformHandle transform_gltf = transform_pool.CreateTransform(float3{ 0, 1, 1 });

	int window_width = 1280;
	int window_height = 720;
	const WindowHandle window = CreateOSWindow(
		BB::OS_WINDOW_STYLE::MAIN,
		250,
		200,
		window_width,
		window_height,
		L"Modern Vulkan");

	RendererCreateInfo render_create_info;
	render_create_info.app_name = "modern vulkan";
	render_create_info.engine_name = "Building Block Engine";
	render_create_info.window_handle = window;
	render_create_info.swapchain_width = static_cast<uint32_t>(window_width);
	render_create_info.swapchain_height = static_cast<uint32_t>(window_height);
	render_create_info.debug = true;
	InitializeRenderer(main_allocator, render_create_info);

	Camera camera{ float3{2.0f, 2.0f, 2.0f}, 0.35f };

	{
		const float4x4 t_ProjMat = Float4x4Perspective(ToRadians(60.0f),
			static_cast<float>(render_create_info.swapchain_width) / static_cast<float>(render_create_info.swapchain_height),
			.001f, 10000.0f);
		BB::SetProjection(t_ProjMat);
	}

	MeshHandle quad_mesh;
	BBStackAllocatorScope(main_allocator)
	{
		//Do some simpel model loading and drawing.
		Vertex vertices[4];
		vertices[0] = { {-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f} };
		vertices[1] = { {0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f} };
		vertices[2] = { {0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f} };
		vertices[3] = { {-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 1.0f, 1.0f} };

		uint32_t indices[] = { 0, 1, 2, 2, 3, 0 };

		CreateMeshInfo quad_create_info{};
		quad_create_info.vertices = Slice(vertices, _countof(vertices));
		quad_create_info.indices = Slice(indices, _countof(indices));
		quad_mesh = CreateMesh(quad_create_info);
	}
	const Model* gltf_model;
	BBStackAllocatorScope(main_allocator)
	{
		gltf_model = Asset::LoadglTFModel(main_allocator, "../resources/models/Duck.gltf");
	}

	InputEvent input_events[INPUT_EVENT_BUFFER_MAX]{};
	size_t input_event_count = 0;

	bool freeze_cam = false;
	bool quit_app = false;
	float delta_time = 0;

	static auto start_time = std::chrono::high_resolution_clock::now();
	auto current_time = std::chrono::high_resolution_clock::now();

	while (!quit_app)
	{
		ProcessMessages(window);
		PollInputEvents(input_events, input_event_count);

		for (size_t i = 0; i < input_event_count; i++)
		{
			const InputEvent& ip = input_events[i];

			if (ip.input_type == INPUT_TYPE::KEYBOARD)
			{
				const KeyInfo& ki = ip.key_info;
				float3 cam_move{};
				if (ki.key_pressed)
					switch (ki.scan_code)
					{
					case KEYBOARD_KEY::F:
						freeze_cam = !freeze_cam;
						break;
					case KEYBOARD_KEY::W:
						cam_move.y = 1;
						break;
					case KEYBOARD_KEY::S:
						cam_move.y = -1;
						break;
					case KEYBOARD_KEY::A:
						cam_move.x = 1;
						break;
					case KEYBOARD_KEY::D:
						cam_move.x = -1;
						break;
					case KEYBOARD_KEY::X:
						cam_move.z = 1;
						break;
					case KEYBOARD_KEY::Z:
						cam_move.z = -1;
						break;
					default:
						break;
					}
				camera.Move(cam_move);
			}
			else if (ip.input_type == INPUT_TYPE::MOUSE)
			{
				const MouseInfo& mi = ip.mouse_info;
				const float2 mouse_move = (mi.move_offset * delta_time) * 0.001f;
				if (!freeze_cam)
					camera.Rotate(mouse_move.x, mouse_move.y);

				if (mi.right_released)
					FreezeMouseOnWindow(window);
				if (mi.left_released)
					UnfreezeMouseOnWindow();
			}
		}
		
		BBStackAllocatorScope(main_allocator)
		{
			BB::SetView(camera.CalculateView());
			StartFrame();

			for (size_t i = 0; i < gltf_model->root_node_count; i++)
			{
				const float4x4 root_matrix = transform_pool.GetTransform(transform_gltf).CreateMatrix();
				DrawglTFNode(gltf_model->root_nodes[i], root_matrix);
			}
			//draw stuff here!
			DrawMesh(quad_mesh, transform_pool.GetTransform(transform_test).CreateMatrix());

			EndFrame();
		}

		delta_time = std::chrono::duration<float, std::chrono::seconds::period>(current_time - start_time).count();
		current_time = std::chrono::high_resolution_clock::now();
	}

	DirectDestroyOSWindow(window);

	return 0;
}
