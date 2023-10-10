#pragma once
#include "Common.h"
#include "Rendererfwd.hpp"
#include "Slice.h"

namespace BB
{
	struct RendererCreateInfo
	{
		WindowHandle window_handle;
		const char* app_name;
		const char* engine_name;
		uint32_t swapchain_width;
		uint32_t swapchain_height;
		bool debug;
	};

	struct CreateMeshInfo
	{
		Slice<Vertex> vertices;
		Slice<const uint32_t> indices;
	};

	void InitializeRenderer(StackAllocator_t& a_stack_allocator, const RendererCreateInfo& a_render_create_info);

	void StartFrame();
	void EndFrame();

	MeshHandle CreateMesh(const CreateMeshInfo& a_create_info);
	void FreeMesh(const MeshHandle a_mesh);

	void DrawMesh(const MeshHandle a_mesh, const float4x4& a_transform);
}