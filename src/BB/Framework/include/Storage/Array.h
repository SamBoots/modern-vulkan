#pragma once
#include "Utils/Logger.h"
#include "BBMemory.h"

#include "Utils/Utils.h"

namespace BB
{
	namespace Array_Specs
	{
		constexpr const size_t multipleValue = 8;
		constexpr const size_t standardSize = 8;
	};

	template<typename T>
	struct Array
	{
		static constexpr bool trivialDestructible_T = std::is_trivially_destructible_v<T>;

		struct Iterator
		{
			//Iterator idea from:
			//https://www.internalpointers.com/post/writing-custom-iterators-modern-cpp

			using value_type = T;
			using pointer = T*;
			using reference = T&;

			Iterator(pointer a_ptr) : m_Ptr(a_ptr) {}

			reference operator*() const { return *m_Ptr; }
			pointer operator->() { return m_Ptr; }

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

			friend bool operator== (const Iterator& a_lhs, const Iterator& a_rhs) { return a_lhs.m_ptr == a_rhs.m_ptr; };
			friend bool operator!= (const Iterator& a_lhs, const Iterator& a_rhs) { return a_lhs.m_ptr != a_rhs.m_ptr; };

			friend bool operator< (const Iterator& a_lhs, const Iterator& a_rhs) { return a_lhs.m_ptr < a_rhs.m_ptr; };
			friend bool operator> (const Iterator& a_lhs, const Iterator& a_rhs) { return a_lhs.m_ptr > a_rhs.m_ptr; };
			friend bool operator<= (const Iterator& a_lhs, const Iterator& a_rhs) { return a_lhs.m_ptr <= a_rhs.m_ptr; };
			friend bool operator>= (const Iterator& a_lhs, const Iterator& a_rhs) { return a_lhs.m_ptr >= a_rhs.m_ptr; };


		private:
			pointer m_Ptr;
		};

		Array(Allocator a_allocator);
		Array(Allocator a_allocator, size_t a_size);
		Array(const Array<T>& a_Array);
		Array(Array<T>&& a_Array) noexcept;
		~Array();

		Array<T>& operator=(const Array<T>& a_rhs);
		Array<T>& operator=(Array<T>&& a_rhs) noexcept;
		T& operator[](const size_t a_Index) const;

		void push_back(T& a_Element);
		void push_back(const T* a_Elements, size_t a_count);
		void insert(size_t a_Position, const T& a_Element);
		void insert(size_t a_Position, const T* a_Elements, size_t a_count);
		template <class... Args>
		void emplace_back(Args&&... a_args);
		template <class... Args>
		void emplace(size_t a_Position, Args&&... a_args);

		void reserve(size_t a_size);
		void resize(size_t a_size);

		void pop();
		void clear();

		const size_t size() const { return m_size; };
		const size_t capacity() const { return m_capacity; }
		T* data() const { return m_Arr; };

		Iterator begin() { return Iterator(m_Arr); }
		Iterator end() { return Iterator(&m_Arr[m_size + 1]); } //Get an out of bounds Iterator.
			 
	private:
		void grow(size_t a_MinCapacity = 0);
		//This function also changes the m_capacity value.
		void reallocate(size_t a_new_capacity);

		Allocator m_allocator;

		T* m_Arr;
		size_t m_size = 0;
		size_t m_capacity;
	};

	template<typename T>
	inline BB::Array<T>::Array(Allocator a_allocator)
		: Array(a_allocator, Array_Specs::standardSize) {}

	template<typename T>
	inline BB::Array<T>::Array(Allocator a_allocator, size_t a_size)
		: m_allocator(a_allocator)
	{
		BB_ASSERT(a_size != 0, "Dynamic_array size is specified to be 0");
		m_capacity = RoundUp(a_size, Array_Specs::multipleValue);

		m_Arr = reinterpret_cast<T*>(BBalloc(m_allocator, m_capacity * sizeof(T)));
	}

	template<typename T>
	inline BB::Array<T>::Array(const Array<T>& a_Array)
	{
		m_allocator = a_Array.m_allocator;
		m_size = a_Array.m_size;
		m_capacity = a_Array.m_capacity;
		m_Arr = reinterpret_cast<T*>(BBalloc(m_allocator, m_capacity * sizeof(T)));

		Memory::Copy<T>(m_Arr, a_Array.m_Arr, m_size);
	}

	template<typename T>
	inline BB::Array<T>::Array(Array<T>&& a_Array) noexcept
	{
		m_allocator = a_Array.m_allocator;
		m_size = a_Array.m_size;
		m_capacity = a_Array.m_capacity;
		m_Arr = a_Array.m_Arr;

		a_Array.m_size = 0;
		a_Array.m_capacity = 0;
		a_Array.m_Arr = nullptr;
		a_Array.m_allocator.allocator = nullptr;
		a_Array.m_allocator.func = nullptr;
	}

	template<typename T>
	inline Array<T>::~Array()
	{
		if (m_Arr != nullptr)
		{
			if constexpr (!trivialDestructible_T)
			{
				for (size_t i = 0; i < m_size; i++)
				{
					m_Arr[i].~T();
				}
			}

			BBfree(m_allocator, m_Arr);
		}
	}

	template<typename T>
	inline Array<T>& BB::Array<T>::operator=(const Array<T>& a_rhs)
	{
		this->~Array();

		m_allocator = a_rhs.m_allocator;
		m_size = a_rhs.m_size;
		m_capacity = a_rhs.m_capacity;
		m_Arr = reinterpret_cast<T*>(BBalloc(m_allocator, m_capacity * sizeof(T)));

		Memory::Copy<T>(m_Arr, a_rhs.m_Arr, m_size);
		
		return *this;
	}

	template<typename T>
	inline Array<T>& BB::Array<T>::operator=(Array<T>&& a_rhs) noexcept
	{
		this->~Array();

		m_allocator = a_rhs.m_allocator;
		m_size = a_rhs.m_size;
		m_capacity = a_rhs.m_capacity;
		m_Arr = a_rhs.m_Arr;

		a_rhs.m_size = 0;
		a_rhs.m_capacity = 0;
		a_rhs.m_Arr = nullptr;
		a_rhs.m_allocator.allocator = nullptr;
		a_rhs.m_allocator.func = nullptr;

		return *this;
	}

	template<typename T>
	inline T& Array<T>::operator[](const size_t a_Index) const
	{
		BB_ASSERT(a_Index <= m_size, "Dynamic_Array, trying to get an element using the [] operator but that element is not there.");
		return m_Arr[a_Index];
	}

	template<typename T>
	inline void Array<T>::push_back(T& a_Element)
	{
		emplace_back(a_Element);
	}

	template<typename T>
	inline void Array<T>::push_back(const T* a_Elements, size_t a_count)
	{
		if (m_size + a_count > m_capacity)
			grow(a_count);

		Memory::Copy<T>(m_Arr, a_Elements, a_count);

		m_size += a_count;
	}

	template<typename T>
	inline void BB::Array<T>::insert(size_t a_Position, const T& a_Element)
	{
		emplace(a_Position, a_Element);
	}

	template<typename T>
	template<class ...Args>
	inline void BB::Array<T>::emplace_back(Args&&... a_args)
	{
		if (m_size >= m_capacity)
			grow();

		new (&m_Arr[m_size]) T(std::forward<Args>(a_args)...);
		m_size++;
	}

	template<typename T>
	template<class ...Args>
	inline void BB::Array<T>::emplace(size_t a_Position, Args&&... a_args)
	{
		BB_ASSERT(m_size >= a_Position, "trying to insert in a position that is bigger then the current Dynamic_Array size!");
		if (m_size >= m_capacity)
			grow();

		if constexpr (!trivialDestructible_T)
		{
			//Move all elements after a_Position 1 to the front.
			for (size_t i = m_size; i > a_Position; i--)
			{
				new (&m_Arr[i]) T(m_Arr[i - 1]);
				m_Arr[i - 1].~T();
			}
		}
		else
		{
			//Move all elements after a_Position 1 to the front.
			//Using memmove for more safety.
			memmove(&m_Arr[a_Position + 1], &m_Arr[a_Position], sizeof(T) * (m_size - a_Position));
		}

		new (&m_Arr[a_Position]) T(std::forward<Args>(a_args)...);
		m_size++;
	}


	template<typename T>
	inline void Array<T>::reserve(size_t a_size)
	{
		if (a_size > m_capacity)
		{
			size_t t_ModifiedCapacity = RoundUp(a_size, Array_Specs::multipleValue);

			reallocate(t_ModifiedCapacity);
			return;
		}
	}

	template<typename T>
	inline void BB::Array<T>::resize(size_t a_size)
	{
		reserve(a_size);

		for (size_t i = m_size; i < a_size; i++)
		{
			new (&m_Arr[i]) T();
		}

		m_size = a_size;
	}

	template<typename T>
	inline void BB::Array<T>::pop()
	{
		BB_ASSERT(m_size != 0, "Dynamic_Array, Popping while m_size is 0!");
		--m_size;
		if constexpr (!trivialDestructible_T)
		{
			m_Arr[m_size].~T();
		}
	}

	template<typename T>
	inline void BB::Array<T>::clear()
	{
		if constexpr (!trivialDestructible_T)
		{
			for (size_t i = 0; i < m_size; i++)
			{
				m_Arr[i].~T();
			}
		}
		m_size = 0;
	}

	template<typename T>
	inline void Array<T>::grow(size_t a_MinCapacity)
	{
		size_t t_ModifiedCapacity = m_capacity * 2;

		if (a_MinCapacity > t_ModifiedCapacity)
			t_ModifiedCapacity = RoundUp(a_MinCapacity, Array_Specs::multipleValue);

		reallocate(t_ModifiedCapacity);
	}

	template<typename T>
	inline void Array<T>::reallocate(size_t a_new_capacity)
	{
		T* t_NewArr = reinterpret_cast<T*>(BBalloc(m_allocator, a_new_capacity * sizeof(T)));

		Memory::Move(t_NewArr, m_Arr, m_size);
		BBfree(m_allocator, m_Arr);

		m_Arr = t_NewArr;
		m_capacity = a_new_capacity;
	}
}