#pragma once

#include "Common.h"
#include "Rendererfwd.hpp"
#include "Slice.h"

#include "Storage/LinkedList.h"
#include "MemoryArena.hpp"

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

	enum class RENDER_PASS_TYPE
	{
		STANDARD_3D
	};

	struct CreateShaderEffectInfo
	{
		const char* name;
		const char* shader_path;
		const char* shader_entry;
		SHADER_STAGE stage;
		SHADER_STAGE_FLAGS next_stages;
		uint32_t push_constant_space;
		RENDER_PASS_TYPE pass_type;
	};

	struct CreateMaterialInfo
	{
		Slice<ShaderEffectHandle> shader_effects;
		RTexture base_color;
		RTexture normal_texture;
	};

	struct UploadTextureInfo
	{
		const char* name;
		const void* pixels;
		IMAGE_FORMAT format;
		uint32_t width;
		uint32_t height;
	};

	//get one pool per thread
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

	class UploadBufferView : public LinkedListNode<UploadBufferView>
	{
		friend class UploadBufferPool;
	public:
		bool AllocateAndMemoryCopy(const void* a_src, const uint32_t a_byte_size, uint32_t& a_out_allocation_offset)
		{
			a_out_allocation_offset = used + offset;
			const uint32_t new_used = used + a_byte_size;
			if (new_used > size) //not enough space, fail the allocation
				return false;

			memcpy(Pointer::Add(view_mem_start, used), a_src, a_byte_size);
			used = new_used;
			return true;
		}

		GPUBuffer GetBufferHandle() const { return upload_buffer_handle; }
		uint32_t UploadBufferViewOffset() const { return offset; }

	private:
		GPUBuffer upload_buffer_handle;	//8
		void* view_mem_start;			//16
		//I suppose we never make an upload buffer bigger then 2-4 gb? test for it on uploadbufferpool creation
		uint32_t offset;				//20
		uint32_t size;					//24
		uint32_t used;					//28
		uint32_t pool_index;			//32
		uint64_t fence_value;			//40
		//LinkedListNode holds next, so //48
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
	void StartFrame(const RCommandList a_list);
	void EndFrame(const RCommandList a_list, bool a_skip = false);

	RenderTarget CreateRenderTarget(MemoryArena& a_arena, const uint2 a_render_target_extent, const char* a_name = "default");
	void ResizeRenderTarget(const RenderTarget render_target, const uint2 a_render_target_extent);
	void StartRenderTarget(const RCommandList a_list, const RenderTarget a_render_target);
	void EndRenderTarget(const RCommandList a_list, const RenderTarget a_render_target);
	RTexture GetCurrentRenderTargetTexture(const RenderTarget a_render_target);

	RenderScene3DHandle Create3DRenderScene(MemoryArena& a_arena, const SceneCreateInfo& a_info);
	void StartRenderScene(const RenderScene3DHandle a_scene);
	void EndRenderScene(const RCommandList a_cmd_list, UploadBufferView& a_upload_buffer_view, const RenderScene3DHandle a_scene, const RenderTarget a_render_target, const uint2 a_draw_area_size, const int2 a_draw_area_offset, const float3 a_clear_color, bool a_skip = false);

	void SetView(const RenderScene3DHandle a_scene, const float4x4& a_view);
	void SetProjection(const RenderScene3DHandle a_scene, const float4x4& a_projection);

	UploadBufferView& GetUploadView(const size_t a_upload_size);
	CommandPool& GetGraphicsCommandPool();
	CommandPool& GetTransferCommandPool();

	bool PresentFrame(const BB::Slice<CommandPool> a_cmd_pools, const BB::Slice<UploadBufferView> a_upload_views);
	bool ExecuteGraphicCommands(const BB::Slice<CommandPool> a_cmd_pools, const BB::Slice<UploadBufferView> a_upload_views);
	bool ExecuteTransferCommands(const BB::Slice<CommandPool> a_cmd_pools, const BB::Slice<UploadBufferView> a_upload_views);
	
	GPUBufferView AllocateFromVertexBuffer(const size_t a_size_in_bytes);
	GPUBufferView AllocateFromIndexBuffer(const size_t a_size_in_bytes);

	WriteableGPUBufferView AllocateFromWritableVertexBuffer(const size_t a_size_in_bytes);
	WriteableGPUBufferView AllocateFromWritableIndexBuffer(const size_t a_size_in_bytes);

	const MeshHandle CreateMesh(const RCommandList a_list, const CreateMeshInfo& a_create_info, UploadBufferView& a_upload_view);
	void FreeMesh(const MeshHandle a_mesh);

	LightHandle CreateLight(const RenderScene3DHandle a_scene, const CreateLightInfo& a_create_info);
	void CreateLights(const RenderScene3DHandle a_scene, const Slice<CreateLightInfo> a_create_infos, LightHandle* const a_light_handles);
	void FreeLight(const RenderScene3DHandle a_scene, const LightHandle a_light);
	PointLight& GetLight(const RenderScene3DHandle a_scene, const LightHandle a_light);

	bool CreateShaderEffect(MemoryArena& a_temp_arena, const Slice<CreateShaderEffectInfo> a_create_infos, ShaderEffectHandle* const a_handles);
	void FreeShaderEffect(const ShaderEffectHandle a_shader_effect);
	bool ReloadShaderEffect(const ShaderEffectHandle a_shader_effect);

	const MaterialHandle CreateMaterial(const CreateMaterialInfo& a_create_info);
	void FreeMaterial(const MaterialHandle a_material);

	const RTexture UploadTexture(const RCommandList a_list, const UploadTextureInfo& a_upload_info, UploadBufferView& a_upload_view);
	void FreeTexture(const RTexture a_texture);

	const GPUBuffer CreateGPUBuffer(const GPUBufferCreateInfo& a_create_info);
	void FreeGPUBuffer(const GPUBuffer a_buffer);
	void* MapGPUBuffer(const GPUBuffer a_buffer);
	void UnmapGPUBuffer(const GPUBuffer a_buffer);

	void DrawMesh(const RenderScene3DHandle a_scene, const MeshHandle a_mesh, const float4x4& a_transform, const uint32_t a_index_start, const uint32_t a_index_count, const MaterialHandle a_material);

	RTexture GetWhiteTexture();
	RTexture GetBlackTexture();
}
