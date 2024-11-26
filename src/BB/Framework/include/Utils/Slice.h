#pragma once

namespace BB
{
	//--------------------------------------------------------
	// A slice is a non-owning reference to N contiguous elements in memory
	// Slice is a way to abstract sending dynamic_array's or stack arrays.
	//--------------------------------------------------------
	template<typename T>
	class Slice
	{
		using DataType = T;
	public:
		struct Iterator
		{
			Iterator(DataType* a_ptr) : m_ptr(a_ptr) {}

			DataType& operator*() const { return *m_ptr; }
			DataType* operator->() { return m_ptr; }

			Iterator& operator++()
			{
				m_ptr++;
				return *this;
			}

			Iterator operator++(int)
			{
				Iterator t_Tmp = *this;
				++(*this);
				return t_Tmp;
			}

			friend bool operator== (const Iterator& a_lhs, const Iterator& a_rhs) { return a_lhs.m_ptr == a_rhs.m_ptr; }
			friend bool operator!= (const Iterator& a_lhs, const Iterator& a_rhs) { return a_lhs.m_ptr != a_rhs.m_ptr; }

			friend bool operator< (const Iterator& a_lhs, const Iterator& a_rhs) { return a_lhs.m_ptr < a_rhs.m_ptr; }
			friend bool operator> (const Iterator& a_lhs, const Iterator& a_rhs) { return a_lhs.m_ptr > a_rhs.m_ptr; }
			friend bool operator<= (const Iterator& a_lhs, const Iterator& a_rhs) { return a_lhs.m_ptr <= a_rhs.m_ptr; }
			friend bool operator>= (const Iterator& a_lhs, const Iterator& a_rhs) { return a_lhs.m_ptr >= a_rhs.m_ptr; }

		private:
			DataType* m_ptr;
		};

		Slice() : m_ptr(nullptr), m_size(0) {}
		Slice(DataType* a_ptr, size_t a_Size): m_ptr(a_ptr), m_size(a_Size) {}
		Slice(DataType* a_Begin, DataType* a_End) : m_ptr(a_Begin), m_size(a_End - a_Begin) {}
		Slice(const Slice<DataType>& a_slice) : m_ptr(a_slice.m_ptr), m_size(a_slice.m_size) {}

		Slice<DataType>& operator=(const Slice<DataType>& a_rhs)
		{
			m_ptr = a_rhs.m_ptr;
			m_size = a_rhs.m_size;
			return *this;
		}

		DataType& operator[](size_t a_Index) const
		{
			BB_ASSERT(m_size > a_Index, "Slice error, trying to access memory");
			return m_ptr[a_Index];
		}

		const Slice SubSlice(size_t a_position, size_t a_Size) const
		{
			BB_ASSERT(m_size > a_position + a_Size - 1, "Subslice error, the subslice has unowned memory.");
			return Slice(m_ptr + a_position, a_Size);
		}

		Iterator begin() const { return Iterator(m_ptr); }
		Iterator end() const { return Iterator(&m_ptr[m_size]); }

		T* data() const { return m_ptr; }
		size_t size() const { return m_size; }
		size_t sizeInBytes() const { return m_size * sizeof(DataType); }

	private:
		DataType* m_ptr;
		size_t m_size;
	};

	template<typename T>
	using ConstSlice = Slice<const T>;
}
