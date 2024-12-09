#pragma once
#include "Utils/Utils.h"
#include "BBMemory.h"
#include "Slice.h"

#include "Common.h"

#include "MemoryArena.hpp"

namespace BB
{
	constexpr uint32_t SPARSE_SET_INVALID = UINT32_MAX;

	class StaticSparseSet
	{
	public:
		StaticSparseSet(const StaticSparseSet& a_map) = delete;
		StaticSparseSet(StaticSparseSet&& a_map) = delete;

		StaticSparseSet& operator=(const StaticSparseSet& a_rhs) = delete;
		StaticSparseSet& operator=(StaticSparseSet&& a_rhs) = delete;

		void Init(MemoryArena& a_arena, const uint32_t a_sparse_size, const uint32_t a_dense_size)
		{
			BB_ASSERT(m_sparse == nullptr, "initializing a static slotmap while it was already initialized or not set to 0");
			m_sparse_max = a_sparse_size;
			m_dense_max = a_dense_size;
			m_dense_count = 0;

			m_sparse = ArenaAllocArr(a_arena, uint32_t, m_sparse_max);
			m_dense = ArenaAllocArr(a_arena, uint32_t, m_dense_max);
			for (size_t i = 0; i < m_sparse_max; i++)
			{
				m_sparse[i] = SPARSE_SET_INVALID;
			}
		}

		// returns SPARSE_SET_INVALID on failure
		uint32_t Find(const uint32_t a_sparse_value)
		{
			if (a_sparse_value > m_sparse_max)
				return SPARSE_SET_INVALID;
			const uint32_t dense_index = m_sparse[a_sparse_value];
			if (dense_index < m_dense_count && m_dense[dense_index] != a_sparse_value)
				return SPARSE_SET_INVALID;

			return dense_index;
		}

		uint32_t insert(const uint32_t a_sparse_value)
		{
			if (Find(a_sparse_value) != SPARSE_SET_INVALID)
				return SPARSE_SET_INVALID;
			if (m_dense_count >= m_dense_max)
				return SPARSE_SET_INVALID;

			m_dense[m_dense_count] = a_sparse_value;
			m_sparse[a_sparse_value] = m_dense_count++;

			return m_sparse[a_sparse_value];
		}

		bool Erase(const uint32_t a_sparse_value)
		{
			if (Find(a_sparse_value) == SPARSE_SET_INVALID)
				return false;

			// TODO: use move here
			const uint32_t move_value = m_dense[--m_dense_count];
			m_dense[m_sparse[a_sparse_value]] = move_value;
			m_sparse[move_value] = m_sparse[a_sparse_value];

			return true;
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

	private:
		uint32_t m_sparse_max;
		uint32_t m_dense_max;
		uint32_t m_dense_count;
		uint32_t* m_sparse;
		uint32_t* m_dense;
	};
}
