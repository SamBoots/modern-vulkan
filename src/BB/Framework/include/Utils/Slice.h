#pragma once
#include "Storage/Array.h"
#include "Storage/Pool.h"

namespace BB
{
	//--------------------------------------------------------
	// A slice is a non-owning reference to N contiguous elements in memory
	// Slice is a way to abstract sending dynamic_array's or stack arrays.
	//--------------------------------------------------------
	template<typename T>
	class Slice
	{
	public:
		struct Iterator
		{
			Iterator(T* a_ptr) : m_Ptr(a_ptr) {}

			T& operator*() const { return *m_Ptr; }
			T* operator->() { return m_Ptr; }

			Iterator& operator++()
			{
				m_Ptr++;
				return *this;
			}

			Iterator operator++(int)
			{
				Iterator t_Tmp = *this;
				++(*this);
				return t_Tmp;
			}

			friend bool operator== (const Iterator& a_lhs, const Iterator& a_rhs) { return a_lhs.m_Ptr == a_rhs.m_Ptr; }
			friend bool operator!= (const Iterator& a_lhs, const Iterator& a_rhs) { return a_lhs.m_Ptr != a_rhs.m_Ptr; }

			friend bool operator< (const Iterator& a_lhs, const Iterator& a_rhs) { return a_lhs.m_Ptr < a_rhs.m_Ptr; }
			friend bool operator> (const Iterator& a_lhs, const Iterator& a_rhs) { return a_lhs.m_Ptr > a_rhs.m_Ptr; }
			friend bool operator<= (const Iterator& a_lhs, const Iterator& a_rhs) { return a_lhs.m_Ptr <= a_rhs.m_Ptr; }
			friend bool operator>= (const Iterator& a_lhs, const Iterator& a_rhs) { return a_lhs.m_Ptr >= a_rhs.m_Ptr; }

		private:
			T* m_Ptr;
		};

		Slice() : m_Ptr(nullptr), m_size(0) {}
		Slice(T* a_ptr, size_t a_Size) : m_Ptr(a_ptr), m_size(a_Size) {}
		Slice(T* a_Begin, T* a_End) : m_Ptr(a_Begin), m_size(a_End - a_Begin) {}
		Slice(Array<T>& a_Array) : m_Ptr(a_Array.data()), m_size(a_Array.size()) {}
		Slice(Pool<T>& a_pool) : m_Ptr(a_pool.data()), m_size(a_pool.size()) {}

		Slice<T>& operator=(const Slice<T>& a_Slice);
		Slice<T>& operator=(const Array<T>& a_rhs);
		Slice<T>& operator=(const Pool<T>& a_rhs);

		T& operator[](size_t a_Index) const
		{
			BB_ASSERT(m_size > a_Index, "Slice error, trying to access memory");
			return m_Ptr[a_Index];
		}

		const Slice SubSlice(size_t a_Position, size_t a_Size) const
		{
			BB_ASSERT(m_size > a_Position + a_Size - 1, "Subslice error, the subslice has unowned memory.");
			return Slice(m_Ptr + a_Position, a_Size);
		}

		Iterator begin() const { return Iterator(m_Ptr); }
		Iterator end() const { return Iterator(&m_Ptr[m_size]); }

		T* data() const { return m_Ptr; };
		size_t size() const { return m_size; }
		size_t sizeInBytes() const { return m_size * sizeof(T); }

	private:
		T* m_Ptr;
		size_t m_size;
	};

	template<typename T>
	inline Slice<T>& BB::Slice<T>::operator=(const Slice<T>& a_rhs)
	{
		m_Ptr = a_rhs.m_Ptr;
		m_size = a_rhs.m_size;
		return *this;
	}

	template<typename T>
	inline Slice<T>& BB::Slice<T>::operator=(const Array<T>& a_rhs)
	{
		m_Ptr = a_rhs.data();
		m_size = a_rhs.size();
		return *this;
	}

	template<typename T>
	inline Slice<T>& BB::Slice<T>::operator=(const Pool<T>& a_rhs)
	{
		m_Ptr = a_rhs.data();
		m_size = a_rhs.size();
		return *this;
	}
}
