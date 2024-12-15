#pragma once
#include "BBMemory.h"
#include "ecs/ECSBase.hpp"
#include "Storage/BBString.h"

namespace BB
{
	using NameComponent = StackString<64>;

	class NameComponentPool
	{
	public:
		void Init(struct MemoryArena& a_arena, const uint32_t a_transform_count);

		bool CreateComponent(const ECSEntity a_entity);
		bool CreateComponent(const ECSEntity a_entity, const NameComponent& a_component);
		bool FreeComponent(const ECSEntity a_entity);
		NameComponent& GetComponent(const ECSEntity a_entity) const;

		inline ECSSignatureIndex GetSignatureIndex() const
		{
			return NAME_ECS_SIGNATURE;
		}
		inline uint32_t GetSize() const
		{
			return m_size;
		}
			
	private:
		bool EntityInvalid(const ECSEntity a_entity) const;

		// components equal to entities.
		uint32_t m_size;
		StaticArray<NameComponent> m_components;
	};
	static_assert(is_ecs_component_map<NameComponentPool, NameComponent>);
}
