#pragma once

//don't care about uninitialized variable warnings
#pragma warning(suppress : 26495) 
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
		void* pixels;
		uint32_t bit_count;
		uint32_t width;
		uint32_t height;
	};

	void InitializeRenderer(StackAllocator_t& a_stack_allocator, const RendererCreateInfo& a_render_create_info);

	void StartFrame();
	void EndFrame();

	void SetView(const float4x4& a_view);
	void SetProjection(const float4x4& a_projection);

	const MeshHandle CreateMesh(const CreateMeshInfo& a_create_info);
	void FreeMesh(const MeshHandle a_mesh);

	const RTexture UploadTexture(const UploadImageInfo& a_upload_info);
	void FreeTexture(const RTexture a_texture);

	void DrawMesh(const MeshHandle a_mesh, const float4x4& a_transform);
}

#pragma warning(default : 26495) 
