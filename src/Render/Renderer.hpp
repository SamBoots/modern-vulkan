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
		Slice<uint32_t> indices;
	};

	using RTexture = FrameworkHandle32Bit<struct RTextureTag>;

	struct UploadImageInfo
	{
		const char* name;
		const void* pixels;
		uint32_t bit_count;
		uint32_t width;
		uint32_t height;
	};

	void InitializeRenderer(StackAllocator_t& a_stack_allocator, const RendererCreateInfo& a_render_create_info);

	void StartFrame();
	void EndFrame();

	void SetView(const float4x4& a_view);
	void SetProjection(const float4x4& a_projection);

	RUploadView GetUploadView(const size_t a_upload_size);

	const MeshHandle CreateMesh(const CreateMeshInfo& a_create_info);
	void FreeMesh(const MeshHandle a_mesh);

	const RTexture UploadTexture(const UploadImageInfo& a_upload_info, const RCommandList a_list, const RUploadView a_upload_view, const uint32_t a_upload_view_offset);
	void FreeTexture(const RTexture a_texture);

	void DrawMesh(const MeshHandle a_mesh, const float4x4& a_transform);
}
