#pragma once
#include "GPUBuffers.hpp"
#include "Rendererfwd.hpp"
#include "ecs/components/RenderComponent.hpp"

#include "ClearStage.hpp"
#include "ShadowMapStage.hpp"
#include "RasterMeshStage.hpp"

namespace BB
{
	struct RenderSystemFrame
	{
		RDescriptorIndex render_target;
		RFence fence;
		uint64_t fence_value;
	};

	class RenderSystem
	{
	public:
		friend class Editor;
		void Init(MemoryArena& a_arena, const uint32_t a_back_buffer_count, const uint32_t a_max_lights, const uint2 a_render_target_size);

		void StartFrame(const RCommandList a_list);
		RenderSystemFrame EndFrame(const RCommandList a_list, const IMAGE_LAYOUT a_current_layout);
		void UpdateRenderSystem(MemoryArena& a_per_frame_arena, const RCommandList a_list, const uint2 a_draw_area, const WorldMatrixComponentPool& a_world_matrices, const RenderComponentPool& a_render_pool, const ConstSlice<LightComponent> a_lights);
		
		void Resize(const uint2 a_new_extent, const bool a_force = false);
		void ResizeNewFormat(const uint2 a_render_target_size, const IMAGE_FORMAT a_render_target_format);
		void Screenshot(const PathString& a_path) const;

		static RDescriptorLayout GetSceneDescriptorLayout();

		bool ToggleSkipSkyboxPass()
		{
			return m_options.skip_skybox = !m_options.skip_skybox;
		}

		bool ToggleSkipShadowMappingPass()
		{
			return m_options.skip_shadow_mapping = !m_options.skip_shadow_mapping;
		}

		bool ToggleSkipObjectRenderingPass()
		{
			return m_options.skip_object_rendering = !m_options.skip_object_rendering;
		}

		bool ToggleSkipBloomPass()
		{
			return m_options.skip_bloom = !m_options.skip_bloom;
		}

		uint2 GetRenderTargetExtent() const
		{
			return m_render_target.extent;
		}

		void SetView(const float4x4& a_view, const float3& a_view_position);
		void SetProjection(const float4x4& a_projection);

        float4x4 GetProjection() const {return m_scene_info.proj; }
        float4x4 GetView() const {return m_scene_info.view; }

	private:
		struct PerFrame
		{
			RDescriptorIndex render_target_view;

			uint2 previous_draw_area;
			GPUFenceValue fence_value;
			DescriptorAllocation scene_descriptor;

			// scene data
			GPUStaticCPUWriteableBuffer scene_buffer;
			// I want this to be uniform but hlsl is giga cringe
			GPULinearBuffer storage_buffer;

			struct Bloom
			{
				RImage image;
				RDescriptorIndex descriptor_index_0;
				RDescriptorIndex descriptor_index_1;
				uint2 resolution;
			} bloom;
		};

		struct RenderTarget
		{
			RImage image;
			uint2 extent;
			IMAGE_FORMAT format;
		};

		void UpdateConstantBuffer(const uint32_t a_frame_index, const RCommandList a_list, const uint2 a_draw_area_size, const ConstSlice<LightComponent> a_lights);
		void ResourceUploadPass(PerFrame& a_pfd, const RCommandList a_list, const DrawList& a_draw_list, const ConstSlice<LightComponent> a_lights);
		void BloomPass(const PerFrame& a_pfd, const RCommandList a_list, const uint2 a_draw_area_size);

		void CreateRenderTarget(const uint2 a_render_target_size);

		uint32_t m_current_frame;
		StaticArray<PerFrame> m_per_frame;
		RenderTarget m_render_target;

		struct Options
		{
			bool skip_skybox;
			bool skip_shadow_mapping;
			bool skip_object_rendering;
			bool skip_bloom;
		} m_options;

		Scene3DInfo m_scene_info;
		struct GlobalBuffer
		{
			GPULinearBuffer buffer;
			uint32_t light_max;
			GPUBufferView light_view;
			GPUBufferView light_viewproj_view;
		} m_global_buffer;

		struct PostFX
		{
			float bloom_strength;
			float bloom_scale;
		} m_postfx;

		RFence m_fence;
		uint64_t m_next_fence_value;
		uint64_t m_last_completed_fence_value;
		GPUUploadRingAllocator m_upload_allocator;

		MasterMaterialHandle m_gaussian_material;

        ClearStage m_clear_stage;
        ShadowMapStage m_shadowmap_stage;
        RasterMeshStage m_raster_mesh_stage;
	};
}
