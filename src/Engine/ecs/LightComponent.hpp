#pragma once
#include "BBMemory.h"
#include "ECSBase.hpp"
#include "Rendererfwd.hpp"
#include "Storage/SparseSet.hpp"

namespace BB
{
	struct LightComponent
	{
		Light light;
		float4x4 projection_view;
	};

	class LightComponentPool
	{
	public:
		void Init(struct MemoryArena& a_arena, const uint32_t a_light_count, const uint32_t a_entity_count);

		bool CreateComponent(const ECSEntity a_entity);
		bool CreateComponent(const ECSEntity a_entity, const LightComponent& a_component);
		bool FreeComponent(const ECSEntity a_entity);
		LightComponent& GetComponent(const ECSEntity a_entity) const;

		inline ECSSignatureIndex GetSignatureIndex() const
		{
			return LIGHT_ECS_SIGNATURE;
		}
		inline uint32_t GetSize() const
		{
			return m_sparse_set.Size();
		}

	private:
		// components equal to entities.
		StaticSparseSet m_sparse_set;
		StaticArray<LightComponent> m_components;
	};
	static_assert(is_ecs_component_map<LightComponentPool, LightComponent>);
}
