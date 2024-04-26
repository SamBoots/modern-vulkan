#pragma once

#include "Common.h"
#include "Rendererfwd.hpp"
#include "Slice.h"

#include "Storage/LinkedList.h"
#include "MemoryInterfaces.hpp"

#include "BBImage.hpp"

namespace BB
{
	enum class RENDER_PASS_TYPE
	{
		STANDARD_3D
	};

	struct CreateShaderEffectInfo
	{
		const char* name;
		const char* shader_entry;
		Buffer shader_data;
		SHADER_STAGE stage;
		SHADER_STAGE_FLAGS next_stages;
		uint32_t push_constant_space;
		RENDER_PASS_TYPE pass_type;
	};

	struct RendererCreateInfo
	{
		WindowHandle window_handle;
		const char* app_name;
		const char* engine_name;
		uint32_t swapchain_width;
		uint32_t swapchain_height;
		bool debug;

		// EXTRA STUFF
		size_t frame_upload_buffer_size = mbSize * 64;
		size_t asset_upload_buffer_size = mbSize * 512;
	};

	struct CreateMeshInfo
	{
		Slice<Vertex> vertices;
		Slice<uint32_t> indices;
	};

	struct WriteTextureInfo
	{
		uint2 extent;
		int2 offset;
		const void* pixels;
		bool set_shader_visible;
	};

	struct CreateMaterialInfo
	{
		const char* name;
		Slice<const ShaderEffectHandle> shader_effects;
	};

	struct CreateTextureInfo
	{
		const char* name;
		IMAGE_FORMAT format;
		IMAGE_USAGE usage;
		uint32_t width;
		uint32_t height;
	};

	// get one pool per thread
	class CommandPool : public LinkedListNode<CommandPool>
	{
		friend class RenderQueue;
		uint32_t m_list_count; //4 
		uint32_t m_list_current_free; //8
		RCommandList* m_lists; //16
		uint64_t m_fence_value; //24
		RCommandPool m_api_cmd_pool; //32
		//LinkedListNode has next ptr value //40 
		bool m_recording; //44
		uint32_t pool_index; //48

		void ResetPool();
	public:
		const RCommandList* GetLists() const { return m_lists; }
		uint32_t GetListsRecorded() const { return m_list_current_free; }
 
		RCommandList StartCommandList(const char* a_name = nullptr);
		void EndCommandList(RCommandList a_list);
	};

	using LightHandle = FrameworkHandle<struct LightHandleTag>;
	struct CreateLightInfo
	{
		//light type here
		float3 pos;
		float3 color;
		float linear_distance;
		float quadratic_distance;
	};

	using RenderScene3DHandle = FrameworkHandle<struct RenderScene3DHandleTag>;
	struct SceneCreateInfo
	{
		float3 ambient_light_color;
		float ambient_light_strength;
		uint32_t light_max;
		uint32_t draw_entry_max;
	};
	using RenderTarget = FrameworkHandle<struct RenderTargetTag>;

	const RenderIO& GetRenderIO();

	bool InitializeRenderer(MemoryArena& a_arena, const RendererCreateInfo& a_render_create_info);
	void RequestResize();

	void GPUWaitIdle();

	GPUDeviceInfo GetGPUInfo(MemoryArena& a_arena);

	struct StartFrameInfo
	{
		float2 mouse_pos;
		float delta_time;
	};

	void StartFrame(const RCommandList a_list, const StartFrameInfo& a_info);
	void EndFrame(const RCommandList a_list, const ShaderEffectHandle a_imgui_vertex, const ShaderEffectHandle a_imgui_fragment, bool a_skip = false);

	RenderTarget CreateRenderTarget(MemoryArena& a_arena, const uint2 a_render_target_extent, const char* a_name = "default");
	void ResizeRenderTarget(const RenderTarget render_target, const uint2 a_render_target_extent);
	void StartRenderTarget(const RCommandList a_list, const RenderTarget a_render_target);
	void EndRenderTarget(const RCommandList a_list, const RenderTarget a_render_target);
	RTexture GetCurrentRenderTargetTexture(const RenderTarget a_render_target);

	RenderScene3DHandle Create3DRenderScene(MemoryArena& a_arena, const SceneCreateInfo& a_info, const char* a_name);
	void StartRenderScene(const RenderScene3DHandle a_scene);
	void EndRenderScene(const RCommandList a_cmd_list, const RenderScene3DHandle a_scene, const RenderTarget a_render_target, const uint2 a_draw_area_size, const int2 a_draw_area_offset, const float3 a_clear_color, bool a_skip = false);

	void SetView(const RenderScene3DHandle a_scene, const float4x4& a_view);
	void SetProjection(const RenderScene3DHandle a_scene, const float4x4& a_projection);

	CommandPool& GetGraphicsCommandPool();
	CommandPool& GetTransferCommandPool();

	bool PresentFrame(const BB::Slice<CommandPool> a_cmd_pools, uint64_t& a_out_fence_value);
	bool ExecuteGraphicCommands(const BB::Slice<CommandPool> a_cmd_pools, uint64_t& a_out_fence_value);

	bool ExecuteAssetTransfer(const BB::Slice<CommandPool> a_cmd_pools, const uint64_t a_asset_transfer_fence_value);
	
	GPUBufferView AllocateFromVertexBuffer(const size_t a_size_in_bytes);
	GPUBufferView AllocateFromIndexBuffer(const size_t a_size_in_bytes);

	WriteableGPUBufferView AllocateFromWritableVertexBuffer(const size_t a_size_in_bytes);
	WriteableGPUBufferView AllocateFromWritableIndexBuffer(const size_t a_size_in_bytes);

	// returns invalid mesh when not enough upload buffer space
	const MeshHandle CreateMesh(const CreateMeshInfo& a_create_info);
	void FreeMesh(const MeshHandle a_mesh);

	LightHandle CreateLight(const RenderScene3DHandle a_scene, const CreateLightInfo& a_create_info);
	void CreateLights(const RenderScene3DHandle a_scene, const Slice<CreateLightInfo> a_create_infos, LightHandle* const a_light_handles);
	void FreeLight(const RenderScene3DHandle a_scene, const LightHandle a_light);
	PointLight& GetLight(const RenderScene3DHandle a_scene, const LightHandle a_light);

	bool CreateShaderEffect(MemoryArena& a_temp_arena, const Slice<CreateShaderEffectInfo> a_create_infos, ShaderEffectHandle* const a_handles);
	bool ReloadShaderEffect(const ShaderEffectHandle a_shader_effect, const Buffer& a_shader);

	const MaterialHandle CreateMaterial(const CreateMaterialInfo& a_create_info);
	void FreeMaterial(const MaterialHandle a_material);

	// returns invalid texture when not enough upload buffer space
	const RTexture CreateTexture(const CreateTextureInfo& a_create_info);

	struct BlitTextureInfo
	{
		RTexture src;
		int2 src_point_0;
		int2 src_point_1;
		bool src_set_shader_visible;

		RTexture dst;
		int2 dst_point_0;
		int2 dst_point_1;
		bool dst_set_shader_visible;
	};

	struct CopyTextureInfo
	{
		uint3 extent;
		RTexture src;
		bool src_set_shader_visible;
		ImageCopyInfo src_copy_info;
		RTexture dst;
		bool dst_set_shader_visible;
		ImageCopyInfo dst_copy_info;
	};

	void BlitTexture(const RCommandList a_list, const BlitTextureInfo& a_blit_info);
	void CopyTexture(const RCommandList a_list, const CopyTextureInfo& a_copy_info);
	void WriteTexture(const RTexture a_texture, const WriteTextureInfo& a_write_info);
	void FreeTexture(const RTexture a_texture);
	bool ReadTexture(const RCommandList a_cmd_list, const RTexture a_texture, const uint2 a_extent, const int2 a_offset, const GPUBuffer a_readback_buffer, const size_t a_readback_buffer_size);

	const GPUBuffer CreateGPUBuffer(const GPUBufferCreateInfo& a_create_info);
	void FreeGPUBuffer(const GPUBuffer a_buffer);
	void* MapGPUBuffer(const GPUBuffer a_buffer);
	void UnmapGPUBuffer(const GPUBuffer a_buffer);

	void DrawMesh(const RenderScene3DHandle a_scene, const MeshHandle a_mesh, const float4x4& a_transform, const uint32_t a_index_start, const uint32_t a_index_count, const RTexture a_base_texture, const RTexture a_normal_texture, const MaterialHandle a_material);

	bool SetDefaultMaterial(const MaterialHandle a_material);
	MaterialHandle GetStandardMaterial();
	RTexture GetWhiteTexture();
	RTexture GetBlackTexture();
	RTexture GetDebugTexture();
}
