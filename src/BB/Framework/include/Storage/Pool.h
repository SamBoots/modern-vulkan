#pragma once
#include "Utils/Logger.h"
#include "Allocators/MemoryArena.hpp"

namespace BB
{
	template<typename T>
	class Pool
	{
	public:
		void CreatePool(MemoryArena& a_arena, const size_t a_size)
		{
			BB_STATIC_ASSERT(sizeof(T) >= sizeof(void*), "Pool object is smaller then the size of a pointer.");
			BB_ASSERT(m_start == nullptr, "Trying to create a pool while one already exists!");

			m_pool = reinterpret_cast<T**>(ArenaAlloc(a_arena, a_size * sizeof(T), alignof(T)));

#ifdef _DEBUG
			m_size = 0;
			m_capacity = a_size;
			m_start = m_pool;
#endif //_DEBUG

			T** pool = m_pool;

			for (size_t i = 0; i < a_size - 1; i++)
			{
				*pool = (reinterpret_cast<T*>(pool)) + 1;
				pool = reinterpret_cast<T**>(*pool);
			}
			*pool = nullptr;
		}

		/// <summary>
		/// Get an object from the pool, returns nullptr if the pool is empty.
		/// </summary>
		T* Get()
		{
			if (m_pool == nullptr)
			{
				BB_WARNING(false, "Trying to get an pool object while there are none left!", WarningType::HIGH);
				return nullptr;
			}

			//Take the freelist
			T* ptr = reinterpret_cast<T*>(m_pool);
			//Set the new head of the freelist.
			m_pool = reinterpret_cast<T**>(*m_pool);

#ifdef _DEBUG
			++m_size;
#endif //_DEBUG

			return ptr;
		}

		/// <summary>
		/// Return an object to the pool.
		/// </summary>
		void Free(T* a_ptr)
		{
#ifdef _DEBUG
			BB_ASSERT((a_ptr >= m_start && a_ptr < Pointer::Add(m_start, m_capacity * sizeof(T))), "Trying to free an pool object that is not part of this pool!");
			--m_size;
#endif // _DEBUG

			//Set the previous free list to the new head.
			(*reinterpret_cast<T**>(a_ptr)) = reinterpret_cast<T*>(m_pool);
			//Set the new head.
			m_pool = reinterpret_cast<T**>(a_ptr);
		}

	private:
#ifdef _DEBUG
		//Debug we can check it's current size.
		size_t m_size;
 		size_t m_capacity;
		void* m_start = nullptr;
#endif // _DEBUG
		T** m_pool;
	};
}
