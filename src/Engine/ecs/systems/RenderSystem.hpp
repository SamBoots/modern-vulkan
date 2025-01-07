#pragma once
#include "GPUBuffers.hpp"
#include "Rendererfwd.hpp"

#include "ecs/components/RenderComponent.hpp"
#include "ecs/components/TransformComponents.hpp"
#include "ecs/components/LightComponent.hpp"

namespace BB
{
	class RenderSystem
	{
	public:
		void Init(MemoryArena& a_arena, const uint32_t a_back_buffer_count, const uint32_t a_max_lights);

		void UpdateLights(MemoryArena& a_temp_arena, const RCommandList a_list, const LightComponentPool& a_light_pool, const ConstSlice<ECSEntity> a_update_lights);
		void UpdateRenderSystem(MemoryArena& a_arena, const RCommandList a_list, const WorldMatrixComponentPool& a_world_matrices, const RenderComponentPool& a_render_pool);
		
	private:
		struct PerFrame
		{
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

		void UpdateConstantBuffer(PerFrame& a_pfd, const RCommandList a_list, const uint2 a_draw_area_size);
		void SkyboxPass(const PerFrame& a_pfd, const RCommandList a_list, const RImageView a_render_target, const uint2 a_draw_area_size, const int2 a_draw_area_offset);
		void ResourceUploadPass(PerFrame& a_pfd, const RCommandList a_list, const DrawList& a_draw_list);
		void ShadowMapPass(const PerFrame& a_pfd, const RCommandList a_list, const uint2 a_shadow_map_resolution, const DrawList& a_draw_list);
		void GeometryPass(const PerFrame& a_pfd, const RCommandList a_list, const RImageView a_render_target, const uint2 a_draw_area_size, const int2 a_draw_area_offset, const DrawList& a_draw_list);
		void BloomPass(const PerFrame& a_pfd, const RCommandList a_list, const RImageView a_render_target, const uint2 a_draw_area_size, const int2 a_draw_area_offset);

		StaticArray<PerFrame> m_per_frame;

		float m_bloom_strength;
		float m_bloom_scale;
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
