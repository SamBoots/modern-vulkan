#pragma once
#include "Rendererfwd.hpp"
#include "Utils/Slice.h"

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

	struct UploadImageInfo
	{
		const char* name;
		const void* pixels;
		IMAGE_FORMAT format;
		IMAGE_USAGE usage;
		uint32_t width;
		uint32_t height;
	};

	struct CreateShaderEffectInfo
	{
		const char* name;
		const char* shader_path;
		const char* shader_entry;
		SHADER_STAGE stage;
		SHADER_STAGE_FLAGS next_stages;
		uint32_t push_constant_space;
	};

	struct CreateMaterialInfo
	{
		Slice<ShaderEffectHandle> shader_effects;
		RTexture base_color;
		RTexture normal_texture;
	};

	bool InitializeRenderer(StackAllocator_t& a_stack_allocator, const RendererCreateInfo& a_render_create_info);

	void Render();

	void SetView(const float4x4& a_view);
	void SetProjection(const float4x4& a_projection);

	const MeshHandle CreateMesh(const RCommandList a_list, const CreateMeshInfo& a_create_info, class UploadBufferView& a_upload_view);
	void FreeMesh(const MeshHandle a_mesh);

	bool CreateShaderEffect(Allocator a_temp_allocator, const Slice<CreateShaderEffectInfo> a_create_infos, ShaderEffectHandle* a_handles);
	void FreeShaderEffect(const ShaderEffectHandle a_shader_effect);

	const MaterialHandle CreateMaterial(const CreateMaterialInfo& a_create_info);
	void FreeMaterial(const MaterialHandle a_material);

	const RTexture UploadTexture(const RCommandList a_list, const UploadImageInfo& a_upload_info, class UploadBufferView& a_upload_view);
	void FreeTexture(const RTexture a_texture);

	void DrawMesh(const MeshHandle a_mesh, const float4x4& a_transform, const uint32_t a_index_start, const uint32_t a_index_count, const MaterialHandle a_material);
}
