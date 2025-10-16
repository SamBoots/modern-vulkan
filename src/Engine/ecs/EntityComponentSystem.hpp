#pragma once
#include "EntityMap.hpp"
#include "systems/RenderSystem.hpp"
#include "components/LightComponent.hpp"
#include "components/NameComponent.hpp"
#include "components/RelationComponent.hpp"

#include "GPUBuffers.hpp"

#include "Math/Math.inl"

namespace BB
{
	// thanks to David Colson for the idea https://www.david-colson.com/2020/02/09/making-a-simple-ecs.html
	struct EntityComponentSystemCreateInfo
	{
		uint32_t render_frame_count;

		uint32_t entity_count;
		uint32_t render_mesh_count;
		uint32_t light_count;
	};

	class EntityComponentSystem
	{
	public:
		friend class Editor;
		bool Init(MemoryArena& a_arena, const EntityComponentSystemCreateInfo& a_create_info, const StackString<32> a_name);

        ECSEntity CreateEntity(const NameComponent& a_name = "#UNNAMED#", const ECSEntity& a_parent = INVALID_ECS_OBJ, const float3 a_position = float3(0.f), const float3x3 a_rotation = Float3x3Identity(), const float3 a_scale = float3(1.f));
        ECSEntity SelectEntityByRay(const float3 a_ray_origin, const float3 a_ray_dir);
        bool DestroyEntity(const ECSEntity a_entity);

        void AddLinesToFrame(const ConstSlice<Line> a_lines);
        void DrawAABB(const ECSEntity a_entity, const LineColor a_color);

		void StartFrame();
		void EndFrame();
		void TransformSystemUpdate();
		RenderSystemFrame RenderSystemUpdate();

		float3 Translate(const ECSEntity a_entity, const float3 a_translate);
		float3x3 Rotate(const ECSEntity a_entity, const float3x3 a_rotate);
		float3 Scale(const ECSEntity a_entity, const float3 a_scale);

		void SetPosition(const ECSEntity a_entity, const float3 a_position);
		void SetRotation(const ECSEntity a_entity, const float3x3 a_rotation);
		void SetScale(const ECSEntity a_entity, const float3 a_scale);

        float3 GetPosition(const ECSEntity a_entity) const;
        float3x3 GetRotation(const ECSEntity a_entity) const;
        float3 GetScale(const ECSEntity a_entity) const;

        bool ValidateEntity(const ECSEntity a_entity) const;

        const float4x4& GetWorldMatrix(const ECSEntity a_entity) const;
        const BoundingBox& GetBoundingBox(const ECSEntity a_entity) const;

        bool EntityAssignBoundingBox(const ECSEntity a_entity, const BoundingBox& a_box);
		bool EntityAssignName(const ECSEntity a_entity, const NameComponent& a_name);
		bool EntityAssignRenderComponent(const ECSEntity a_entity, const RenderComponent& a_draw_info);
		bool EntityAssignLight(const ECSEntity a_entity, const LightComponent& a_light);
        bool EntityAssignRaytraceComponent(const ECSEntity a_entity, const RaytraceComponent& a_raytrace);
		bool EntityFreeLight(const ECSEntity a_entity);

        void CalculateView(const float3 a_pos, const float3 a_center, const float3 a_up);

		RenderSystem& GetRenderSystem()
		{
			return m_render_system;
		}

		StackString<32> GetName() const { return m_name; }

	private:
        void FindECSEntityClickTraverse(const ECSEntity a_entity, const float3& a_ray_origin, const float3& a_ray_dir, ECSEntity& a_found, float& a_found_dist);
		bool AddEntityRelation(const ECSEntity a_entity, const ECSEntity a_parent);
		void UpdateTransform(const ECSEntity a_entity);

		StackString<32> m_name;
		MemoryArena m_per_frame_arena;
		uint32_t m_current_frame;
        uint32_t m_frame_count;

		// ecs entities
		EntityMap m_ecs_entities;

		// component pools
		RelationComponentPool m_relations;
		PositionComponentPool m_positions;
		RotationComponentPool m_rotations;
		ScaleComponentPool m_scales;
        BoundingBoxComponentPool m_bounding_box_pool;
		LocalMatrixComponentPool m_local_matrices;
		WorldMatrixComponentPool m_world_matrices;
		NameComponentPool m_name_pool;
		RenderComponentPool m_render_mesh_pool;
		LightComponentPool m_light_pool;
        RaytraceComponentPool m_raytrace_pool;

		// systems
		RenderSystem m_render_system;
		struct TransformSystem
		{
			EntitySparseSet dirty_transforms;
		} m_transform_system;

		struct RootEntitySystem
		{
			EntitySparseSet root_entities;
		} m_root_entity_system;
	};
}
