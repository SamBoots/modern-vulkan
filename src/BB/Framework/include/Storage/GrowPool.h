#pragma once
#include "Utils/Logger.h"
#include "Allocators/BackingAllocator.h"

namespace BB
{
	/// <summary>
	/// ONLY USE THIS FOR BIG ALLOCATIONS. It uses virtual alloc to extend the pool.
	/// NON_RAII and does not support POD types. 
	/// You must call the destructor and constructor yourself.
	/// Create the pool by using CreatePool and DestroyPool. 
	/// A special container that is not responsible for it's own deallocation. 
	/// It uses virtual alloc in the background.
	/// </summary>
	template<typename T>
	class GrowPool
	{
	public:
		GrowPool() {};
#ifdef _DEBUG
		~GrowPool();
#endif _DEBUG

		//just delete these for safety, copies might cause errors.
		GrowPool(const GrowPool&) = delete;
		GrowPool(const GrowPool&&) = delete;
		GrowPool& operator =(const GrowPool&) = delete;
		GrowPool& operator =(GrowPool&&) = delete;

		/// <summary>
		/// Create a pool that can hold members equal to a_size.
		/// Will likely over allocate more due to how virtual memory paging works.
		/// </summary>
		void CreatePool(const size_t a_size);
		void DestroyPool();

		/// <summary>
		/// Get an object from the pool, returns nullptr if the pool is empty.
		/// </summary>
		T* Get();
		/// <summary>
		/// Return an object to the pool.
		/// </summary>
		void Free(T* a_Ptr);

	private:
#ifdef _DEBUG
		//Debug we can check it's current size.
		size_t m_size = 0;
		size_t m_capacity = 0;
#endif // _DEBUG

		void* m_start = nullptr;
		T** m_Pool = nullptr;
	};

#ifdef _DEBUG
	template<typename T>
	inline GrowPool<T>::~GrowPool()
	{
		BB_ASSERT(m_start == nullptr, "Memory pool was not destroyed before it went out of scope!");
	}
#endif _DEBUG

	template<typename T>
	inline void GrowPool<T>::CreatePool(const size_t a_size)
	{
		BB_STATIC_ASSERT(sizeof(T) >= sizeof(void*), "Pool object is smaller then the size of a pointer.");
		BB_ASSERT(m_start == nullptr, "Trying to create a pool while one already exists!");

		size_t t_AllocSize = a_size * sizeof(T);
		m_start = mallocVirtual(m_start, t_AllocSize);
		m_Pool = reinterpret_cast<T**>(m_start);
		const size_t t_SpaceForElements = t_AllocSize / sizeof(T);

#ifdef _DEBUG
		m_size = 0;
		m_capacity = t_SpaceForElements;
#endif //_DEBUG

		T** t_Pool = m_Pool;

		for (size_t i = 0; i < t_SpaceForElements - 1; i++)
		{
			*t_Pool = (reinterpret_cast<T*>(t_Pool)) + 1;
			t_Pool = reinterpret_cast<T**>(*t_Pool);
		}
		*t_Pool = nullptr;
	}

	template<typename T>
	inline void GrowPool<T>::DestroyPool()
	{
		freeVirtual(m_start);
#ifdef _DEBUG
		//Set everything to 0 in debug to indicate it was destroyed.
		memset(this, 0, sizeof(BB::GrowPool<T>));
#endif //_DEBUG
		m_start = nullptr;
	}

	template<class T>
	inline T* GrowPool<T>::Get()
	{
		if (*m_Pool == nullptr)
		{
			BB_WARNING(false, "Growing the growpool, if this happens often try to reserve more.", WarningType::OPTIMALIZATION);
			//get more memory!
			size_t alloc_size = 8 * sizeof(T);
			mallocVirtual(m_start, alloc_size);
			const size_t elements_allocated = Max(static_cast<size_t>(8), static_cast<size_t>(alloc_size / sizeof(T)));

#ifdef _DEBUG
			m_capacity += elements_allocated;
#endif //_DEBUG

			T** t_Pool = m_Pool;

			for (size_t i = 0; i < elements_allocated - 1; i++)
			{
				*t_Pool = (reinterpret_cast<T*>(t_Pool)) + 1;
				t_Pool = reinterpret_cast<T**>(*t_Pool);
			}
			*t_Pool = nullptr;
		}

		//Take the freelist
		T* t_Ptr = reinterpret_cast<T*>(m_Pool);
		//Set the new head of the freelist.
		m_Pool = reinterpret_cast<T**>(*m_Pool);

#ifdef _DEBUG
		++m_size;
#endif //_DEBUG

		return t_Ptr;
	}

	template<typename T>
	inline void GrowPool<T>::Free(T* a_Ptr)
	{
#ifdef _DEBUG
		BB_ASSERT((a_Ptr >= m_start && a_Ptr < Pointer::Add(m_start, m_capacity * sizeof(T))), "Trying to free an pool object that is not part of this pool!");
		--m_size;
#endif // _DEBUG

		//Set the previous free list to the new head.
		(*reinterpret_cast<T**>(a_Ptr)) = reinterpret_cast<T*>(m_Pool);
		//Set the new head.
		m_Pool = reinterpret_cast<T**>(a_Ptr);
	}
}