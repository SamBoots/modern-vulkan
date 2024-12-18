#pragma once
#include "EntityMap.hpp"
#include "components/LightComponent.hpp"
#include "components/NameComponent.hpp"
#include "components/RenderComponent.hpp"
#include "components/TransformComponent.hpp"

namespace BB
{
	class EntityComponentSystem
	{
	public:

		template <typename COMPONENT_TYPE>
		COMPONENT_TYPE GetComponent(const ECSEntity a_entity);

	private:
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
	};
}
