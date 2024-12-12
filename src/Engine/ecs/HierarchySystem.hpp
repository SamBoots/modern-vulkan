#pragma once
#include "BBMemory.h"
#include "ECSBase.hpp"
#include "Rendererfwd.hpp"
#include "Storage/SparseSet.hpp"

namespace BB
{
	class HierarchySystem
	{
	public:
		void Init(MemoryArena& a_arena, const uint32_t a_root_entities_count, const uint32_t a_entity_count);

		bool AddEntity(const ECSEntity a_entity);
		bool RemoveEntity(const ECSEntity a_entity);

		bool AddChilderen(const ECSEntity a_entity, ConstSlice<ECSEntity> a_children);

		bool Update(const float a_delta_time);

		bool IsDirty() const;

	private:
		struct HierarchyEntry
		{
			ECSEntity entity;
			uint32_t child_start;
			uint32_t child_count;
		};
	}
};
