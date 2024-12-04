#pragma once
#include "Utils/Utils.h"
#include "BBMemory.h"
#include "Slice.h"

#include "Common.h"

#include "MemoryArena.hpp"

namespace BB
{
	class StaticSparseSet
	{
		BB_STATIC_ASSERT(std::is_integral<T>, "SparseSet is not integral");
	public:
		StaticSpareSet(const StaticSpareSet<T>& a_map) = delete;
		StaticSpareSet(StaticSpareSet<T>&& a_map) = delete;

		StaticSpareSet<T>& operator=(const StaticSpareSet<T>& a_rhs) = delete;
		StaticSpareSet<T>& operator=(StaticSpareSet<T>&& a_rhs) = delete;

		void Init(MemoryArena& a_arena, const uint32_t a_sparse_size, const uint32_t a_dense_size)
		{
			BB_ASSERT(m_id_arr == nullptr, "initializing a static slotmap while it was already initialized or not set to 0");
			m_sparse_max = a_sparse_size;
			m_dense_max = a_dense_size;
			m_dense_count = 0;

			m_sparse = ArenaAllocArr(a_arena, uint32_t, m_sparse_max);
			m_dense = ArenaAllocArr(a_arena, T, m_dense_max);
			for (size_t i = 0; i < m_sparse_max; i++)
			{
				m_sparse[i] = UINT32_MAX;
			}
		}

		// returns UINT32_MAX on failure
		uint32_t Find(const uint32_t a_sparse_value)
		{
			if (a_sparse_value > m_sparse_max)
				return UINT32_MAX;
			const uint32_t dense_index = m_sparse[a_sparse_value];
			if (dense_index < m_dense_count && m_dense[dense_index] != a_sparse_value)
				return UINT32_MAX;

			return dense_index;
		}

		bool insert(const uint32_t a_sparse_value)
		{
			if (Find(a_sparse_value) != UINT32_MAX)
				return false;
			if (m_dense_count >= m_dense_max)
				return false;

			m_dense[m_dense_count] = a_sparse_value;
			m_sparse[a_sparse_value] = m_dense_count++;

			return true;
		}

		bool Erase(const uint32_t a_sparse_value)
		{
			if (Find(a_sparse_value) == UINT32_MAX)
				return false;

			// TODO: use move here
			const uint32_t move_value = m_dense[--m_dense_count];
			m_dense[m_sparse[a_sparse_value]] = move_value;
			m_sparse[temp] = m_sparse[a_sparse_value];

			return true;
		}


	private:
		uint32_t m_sparse_max;
		uint32_t m_dense_max;
		uint32_t m_dense_count;
		uint32_t* m_sparse;
		T* m_dense;
	};
}
