#pragma once
#include "GPUBuffers.hpp"
#include "Rendererfwd.hpp"

#include "ecs/components/RenderComponent.hpp"
#include "ecs/components/TransformComponents.hpp"
#include "ecs/components/LightComponent.hpp"

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
		
		void Resize(const uint2 a_new_extent);
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
		void SetClearColor(const float3 a_clear_color);

	private:
		struct PerFrame
		{
			RDescriptorIndex render_target_view;

			uint2 previous_draw_area;
			uint64_t fence_value;
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

			RImage depth_image;
			RImageView depth_image_view;
			struct ShadowMap
			{
				RImage image;
				RDescriptorIndex descriptor_index;
				StaticArray<RImageView> render_pass_views;
			} shadow_map;
		};

		struct DrawList
		{
			struct DrawEntry
			{
				Mesh mesh;
				MasterMaterialHandle master_material;
				MaterialHandle material;
				uint32_t index_start;
				uint32_t index_count;
			};

			StaticArray<DrawEntry> draw_entries;
			StaticArray<ShaderTransform> transforms;
		};

		struct RenderTarget
		{
			RImage image;
			uint2 extent;
		};

		void UpdateConstantBuffer(PerFrame& a_pfd, const RCommandList a_list, const uint2 a_draw_area_size, const ConstSlice<LightComponent> a_lights);
		void SkyboxPass(const PerFrame& a_pfd, const RCommandList a_list, const uint2 a_draw_area_size);
		void ResourceUploadPass(PerFrame& a_pfd, const RCommandList a_list, const DrawList& a_draw_list, const ConstSlice<LightComponent> a_lights);
		void ShadowMapPass(const PerFrame& a_pfd, const RCommandList a_list, const uint2 a_shadow_map_resolution, const DrawList& a_draw_list, const ConstSlice<LightComponent> a_lights);
		void GeometryPass(const PerFrame& a_pfd, const RCommandList a_list, const uint2 a_draw_area_size, const DrawList& a_draw_list);
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

		float3 m_clear_color;
		RDescriptorIndex m_skybox_descriptor_index;
		RImage m_skybox;
		MasterMaterialHandle m_skybox_material;
		MasterMaterialHandle m_shadowmap_material;
		MasterMaterialHandle m_gaussian_material;
	};
}
