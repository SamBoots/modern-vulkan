#pragma once
#include "BBMemory.h"

namespace BB
{
	/// <summary>
	/// The gangster little brother of pool.h, really this is part meme.
	/// </summary>
	template<typename T>
	class RawPool
	{
	public:
		RawPool(Allocator a_allocator, const size_t a_size)
		{
			BB_STATIC_ASSERT(sizeof(T) >= sizeof(void*), "Pool object is smaller then the size of a pointer.");
			static_assert(sizeof(T) >= sizeof(void*));
			m_pool = reinterpret_cast<T**>(new T[a_size]{});

			T** pool = m_pool;

			for (size_t i = 0; i < a_size - 1; i++)
			{
				*pool = (reinterpret_cast<T*>(pool)) + 1;
				pool = reinterpret_cast<T**>(*pool);
			}
			*pool = nullptr;
		};

		//We do no copying
		RawPool(const RawPool&) = delete;
		RawPool& operator =(const RawPool&) = delete;

		//We do moving however
		RawPool(RawPool&& a_pool)
		{
			m_pool = a_pool.m_pool;
			m_pool = nullptr;
		}
		RawPool& operator =(RawPool&& a_rhs)
		{
			m_pool = a_rhs.m_pool;
			a_rhs.m_pool = nullptr;
			return *this;
		}
		T* Get()
		{
			if (m_pool == nullptr)
			{
				BB_WARNING(false, "Trying to get an pool object while there are none left!", WarningType::HIGH);
				return nullptr;
			}

			T* ptr = reinterpret_cast<T*>(m_pool);
			m_pool = reinterpret_cast<T**>(*m_pool);

			return ptr;
		}
		//ways to check if this is in the same memory? No! Try Pool instead of rawpool
		void Free(T* a_ptr)
		{
			//Set the previous free list to the new head.
			(*reinterpret_cast<T**>(a_ptr)) = reinterpret_cast<T*>(m_pool);
			//Set the new head.
			m_pool = reinterpret_cast<T**>(a_ptr);
		}

	private:
		T** m_pool;
	};
}