#pragma once
#include "Utils/Logger.h"
#include "BBMemory.h"

namespace BB
{
	/// <summary>
	/// NON_RAII and does not support non-POD types. 
	/// You must call the destructor and constructor yourself.
	/// Create the pool by using CreatePool and DestroyPool. 
	/// A special container that is not responsible for it's own deallocation. 
	/// It has enough memory to hold element T equal to the size given. 
	/// </summary>
	template<typename T>
	class Pool
	{
	public:
#ifdef _DEBUG
		//Must have a constructor because of the debug destructor
		Pool() {}
		~Pool();
#endif //_DEBUG

		//We do no copying
		Pool(const Pool&) = delete;
		Pool& operator =(const Pool&) = delete;

		//We do moving however
		Pool(Pool&& a_pool);
		Pool& operator =(Pool&& a_rhs);

		void CreatePool(Allocator a_allocator, const size_t a_size);
		void DestroyPool(Allocator a_allocator);

		/// <summary>
		/// Get an object from the pool, returns nullptr if the pool is empty.
		/// </summary>
		T* Get();
		/// <summary>
		/// Return an object to the pool.
		/// </summary>
		void Free(T* a_ptr);

		T* data() const { return reinterpret_cast<T*>(m_start); }

	private:
#ifdef _DEBUG
		//Debug we can check it's current size.
		size_t m_size;
 		size_t m_capacity;
		//Check if we use the same allocator for removal.
		Allocator m_allocator{};
#endif // _DEBUG

		void* m_start = nullptr;
		T** m_pool;
	};

#ifdef _DEBUG
	template<typename T>
	inline Pool<T>::~Pool()
	{
		BB_ASSERT(m_start == nullptr, "Memory pool was not destroyed before it went out of scope!");
	}
#endif // _DEBUG

	template<typename T>
	inline Pool<T>::Pool(Pool&& a_pool)
	{
		m_start = a_pool.m_start;
		m_pool = a_pool.m_pool;
		m_start = nullptr;
		m_pool = nullptr;

#ifdef _DEBUG
		m_size = a_pool.m_size;
		m_capacity = a_pool.m_capacity;
		m_allocator = a_pool.m_allocator;
		a_pool.m_size = 0;
		a_pool.m_capacity = 0;
		a_pool.m_allocator.allocator = nullptr;
		a_pool.m_allocator.func = nullptr;
#endif // _DEBUG
	}

	template<typename T>
	inline Pool<T>& Pool<T>::operator=(Pool&& a_rhs)
	{
		m_start = a_rhs.m_start;
		m_pool = a_rhs.m_pool;
		a_rhs.m_start = nullptr;
		a_rhs.m_pool = nullptr;

#ifdef _DEBUG
		m_size = a_rhs.m_size;
		m_capacity = a_rhs.m_capacity;
		m_allocator = a_rhs.m_allocator;
		a_rhs.m_size = 0;
		a_rhs.m_capacity = 0;
		a_rhs.m_allocator.allocator = nullptr;
		a_rhs.m_allocator.func = nullptr;
#endif // _DEBUG

		return *this;
	}

	template<typename T>
	inline void Pool<T>::CreatePool(Allocator a_allocator, const size_t a_size)
	{
		BB_STATIC_ASSERT(sizeof(T) >= sizeof(void*), "Pool object is smaller then the size of a pointer.");
		BB_ASSERT(m_start == nullptr, "Trying to create a pool while one already exists!");

#ifdef _DEBUG
		m_size = 0;
		m_capacity = a_size;
		m_allocator = a_allocator;
#endif //_DEBUG

		m_start = BBalloc(a_allocator, a_size * sizeof(T));
		m_pool = reinterpret_cast<T**>(m_start);

		T** t_Pool = m_pool;

		for (size_t i = 0; i < a_size - 1; i++)
		{
			*t_Pool = (reinterpret_cast<T*>(t_Pool)) + 1;
			t_Pool = reinterpret_cast<T**>(*t_Pool);
		}
		*t_Pool = nullptr;
	}

	template<typename T>
	inline void Pool<T>::DestroyPool(Allocator a_allocator)
	{
#ifdef _DEBUG
		BB_ASSERT(m_allocator.allocator == a_allocator.allocator, "Trying to delete a pool with an allocator that wasn't used in it's CreatePool function.");
#endif //_DEBUG
		BBfree(a_allocator, m_start);
#ifdef _DEBUG
		//Set everything to 0 in debug to indicate it was destroyed.
		memset(this, 0, sizeof(BB::Pool<T>));
#endif //_DEBUG
		m_start = nullptr;
	}

	template<class T>
	inline T* Pool<T>::Get()
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

	template<typename T>
	inline void BB::Pool<T>::Free(T* a_ptr)
	{
#ifdef _DEBUG
		BB_ASSERT((a_ptr >= m_start && a_ptr < Pointer::Add(m_start,  m_capacity * sizeof(T))), "Trying to free an pool object that is not part of this pool!");
		--m_size;
#endif // _DEBUG

		//Set the previous free list to the new head.
		(*reinterpret_cast<T**>(a_ptr)) = reinterpret_cast<T*>(m_pool);
		//Set the new head.
		m_pool = reinterpret_cast<T**>(a_ptr);
	}
}
