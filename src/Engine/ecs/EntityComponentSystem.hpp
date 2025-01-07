#pragma once
#include "EntityMap.hpp"
#include "systems/RenderSystem.hpp"
#include "components/LightComponent.hpp"
#include "components/NameComponent.hpp"

#include "GPUBuffers.hpp"

#include "Math.inl"

namespace BB
{
	// thanks to David Colson for the idea https://www.david-colson.com/2020/02/09/making-a-simple-ecs.html
	struct EntityComponentSystemCreateInfo
	{
		uint32_t entity_count;
		uint32_t render_mesh_count;
		uint32_t light_count;
	};

	class EntityComponentSystem
	{
	public:
		bool Init(MemoryArena& a_arena, const EntityComponentSystemCreateInfo& a_create_info);

		ECSEntity CreateEntity(const NameComponent& a_name = "#UNNAMED#", const ECSEntity& a_parent = INVALID_ECS_OBJ, const float3 a_position = float3(0.f), const float3x3 a_rotation = Float3x3Identity(), const float3 a_scale = float3(1.f));

		void TransformSystemUpdate();
		void RenderSystemUpdate(const RCommandList a_list, const uint32_t a_back_buffer_index);

		float3 Translate(const ECSEntity a_entity, const float3 a_translate);
		float3x3 Rotate(const ECSEntity a_entity, const float3x3 a_rotate);
		float3 Scale(const ECSEntity a_entity, const float3 a_scale);

		void SetPosition(const ECSEntity a_entity, const float3 a_position);
		void SetRotation(const ECSEntity a_entity, const float3x3 a_rotation);
		void SetScale(const ECSEntity a_entity, const float3 a_scale);

		bool EntityAssignName(const ECSEntity a_entity, const NameComponent& a_name);
		bool EntityAssignRenderComponent(const ECSEntity a_entity, const RenderComponent& a_draw_info);
		bool EntityAssignLight(const ECSEntity a_entity, const LightComponent& a_light);

	private:
		void UpdateTransform(const ECSEntity a_entity);

		EntityMap m_ecs_entities;

		// component pools
		PositionComponentPool m_positions;
		RotationComponentPool m_rotations;
		ScaleComponentPool m_scales;
		LocalMatrixComponentPool m_local_matrices;
		WorldMatrixComponentPool m_world_matrices;
		NameComponentPool m_name_pool;
		RenderComponentPool m_render_mesh_pool;
		LightComponentPool m_light_pool;

		struct PerFrame
		{
			MemoryArena arena;
		};
		FixedArray<PerFrame, 3> m_per_frame;
		RenderSystem m_render_system;

		struct TransformSystem
		{
			EntitySparseSet dirty_transforms;
		} m_transform_system;
	};
}
