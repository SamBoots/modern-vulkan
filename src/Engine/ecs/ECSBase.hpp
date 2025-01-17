#pragma once
#include "Enginefwd.hpp"
#include "Storage/Array.h"
#include "Storage/SparseSet.hpp"

namespace BB
{
	// transform
	constexpr ECSSignatureIndex POSITION_ECS_SIGNATURE = ECSSignatureIndex(1);
	constexpr ECSSignatureIndex ROTATION_ECS_SIGNATURE = ECSSignatureIndex(2);
	constexpr ECSSignatureIndex SCALE_ECS_SIGNATURE = ECSSignatureIndex(3);
	constexpr ECSSignatureIndex LOCAL_MATRIX_ECS_SIGNATURE = ECSSignatureIndex(4);
	constexpr ECSSignatureIndex WORLD_MATRIX_ECS_SIGNATURE = ECSSignatureIndex(5);

	constexpr ECSSignatureIndex NAME_ECS_SIGNATURE = ECSSignatureIndex(6);
	constexpr ECSSignatureIndex RENDER_ECS_SIGNATURE = ECSSignatureIndex(7);
	constexpr ECSSignatureIndex LIGHT_ECS_SIGNATURE = ECSSignatureIndex(8);

	class EntitySparseSet
	{
	public:
		EntitySparseSet() = default;
		EntitySparseSet(const EntitySparseSet& a_map) = delete;
		EntitySparseSet(EntitySparseSet&& a_map) = delete;

		EntitySparseSet& operator=(const EntitySparseSet& a_rhs) = delete;
		EntitySparseSet& operator=(EntitySparseSet&& a_rhs) = delete;

		void Init(MemoryArena& a_arena, const uint32_t a_sparse_size, const uint32_t a_dense_size)
		{
			BB_ASSERT(m_sparse == nullptr, "initializing a static slotmap while it was already initialized or not set to 0");
			m_sparse_max = a_sparse_size;
			m_dense_max = a_dense_size;
			m_dense_count = 0;

			m_sparse = ArenaAllocArr(a_arena, uint32_t, m_sparse_max);
			m_dense_ecs = ArenaAllocArr(a_arena, ECSEntity, m_dense_max);
			for (uint32_t i = 0; i < m_sparse_max; i++)
				m_sparse[i] = SPARSE_SET_INVALID;
			for (uint32_t i = 0; i < m_dense_count; i++)
				m_dense_ecs[i] = INVALID_ECS_OBJ;
		}

		ECSEntity operator[](const uint32_t a_index) const
		{
			BB_ASSERT(a_index <= m_dense_max, "trying to get an element using the [] operator but that element is not there.");
			return m_dense_ecs[a_index];
		}

		// returns SPARSE_SET_INVALID on failure
		ECSEntity Find(const uint32_t a_ecs_index) const
		{
			if (a_ecs_index > m_sparse_max)
				return INVALID_ECS_OBJ;
			const uint32_t dense_index = m_sparse[a_ecs_index];
			if (dense_index < m_dense_count && m_dense_ecs[dense_index].index != a_ecs_index)
				return INVALID_ECS_OBJ;

			return m_dense_ecs[dense_index];
		}

		uint32_t Insert(const ECSEntity a_value)
		{
			if (Find(a_value.index) != INVALID_ECS_OBJ)
				return SPARSE_SET_ALREADY_SET;
			if (m_dense_count >= m_dense_max)
				return SPARSE_SET_INVALID;

			m_dense_ecs[m_dense_count] = a_value;
			m_sparse[a_value.index] = m_dense_count++;

			return m_sparse[a_value.index];
		}

		bool Erase(const ECSEntity a_value)
		{
			if (Find(a_value.index) == INVALID_ECS_OBJ)
				return false;

			// TODO: use move here
			const ECSEntity move_value = m_dense_ecs[--m_dense_count];
			m_dense_ecs[m_sparse[a_value.index]] = move_value;
			m_sparse[move_value.index] = m_sparse[a_value.index];

			return true;
		}

		void Clear()
		{
			for (uint32_t i = 0; i < m_sparse_max; i++)
				m_sparse[i] = SPARSE_SET_INVALID;
			for (uint32_t i = 0; i < m_dense_count; i++)
				m_dense_ecs[i] = INVALID_ECS_OBJ;
		}

		uint32_t Size() const
		{
			return m_dense_count;
		}

		uint32_t CapacitySparse() const
		{
			return m_sparse_max;
		}

		uint32_t CapacityDense() const
		{
			return m_dense_max;
		}

		ConstSlice<ECSEntity> GetDense() const
		{
			return ConstSlice<ECSEntity>(m_dense_ecs, m_dense_count);
		}

	private:
		uint32_t m_sparse_max;
		uint32_t m_dense_max;
		uint32_t m_dense_count;
		uint32_t* m_sparse;
		ECSEntity* m_dense_ecs;
	};

	template <typename T, typename Component>
	concept is_ecs_component_map = requires(T v, const Component& a_component, const ECSEntity a_entity)
	{
		// return false if entity already exists
		{ v.CreateComponent(a_entity) } -> std::same_as<bool>;
		// return false if entity already exists
		{ v.CreateComponent(a_entity, a_component) } -> std::same_as<bool>;
		// return false if entity does not exist here
		{ v.FreeComponent(a_entity) } -> std::same_as<bool>;
		// return false if entity does not hold this component
		{ v.GetComponent(a_entity) } -> std::same_as<Component&>;

		{ v.GetSize() } -> std::same_as<uint32_t>;
		{ v.GetSignatureIndex() } -> std::same_as<ECSSignatureIndex>;
	};

	template <typename T, ECSSignatureIndex ECS_INDEX>
	class ECSComponentBase
	{
	public:
		void Init(struct MemoryArena& a_arena, const uint32_t a_entity_max)
		{
			m_components.Init(a_arena, a_entity_max);
			m_components.resize(a_entity_max);
		}

		bool CreateComponent(const ECSEntity a_entity)
		{
			if (EntityInvalid(a_entity))
				return false;

			new (&m_components[a_entity.index]) T();
			return true;
		}

		bool CreateComponent(const ECSEntity a_entity, const T& a_component)
		{
			if (EntityInvalid(a_entity))
				return false;

			new (&m_components[a_entity.index]) T(a_component);
			return true;
		}
		bool FreeComponent(const ECSEntity a_entity)
		{
			if (EntityInvalid(a_entity))
				return false;

			return true;
		}
		T& GetComponent(const ECSEntity a_entity) const
		{
			BB_ASSERT(!EntityInvalid(a_entity), "entity entry is not valid!");
			return m_components[a_entity.index];
		}

		inline ECSSignatureIndex GetSignatureIndex() const
		{
			return ECS_INDEX;
		}
		inline uint32_t GetSize() const
		{
			return m_size;
		}

	private:
		bool EntityInvalid(const ECSEntity a_entity) const
		{
			if (a_entity.index >= m_components.size())
				return true;
			return false;
		}


		// components equal to entities.
		uint32_t m_size;
		StaticArray<T> m_components;
	};
}
