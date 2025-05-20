#pragma once
#include "Utils/Logger.h"
#include "Slice.h"

namespace BB
{
	template<typename T, size_t arr_size>
	class FixedArray
	{
	public:
		FixedArray() = default;
		FixedArray(const FixedArray<T, arr_size>& a_array)
		{
			Memory::Copy<T>(m_arr, a_array.m_arr, arr_size);
		}

		FixedArray<T, arr_size>& operator=(const FixedArray<T, arr_size>& a_rhs)
		{
			this->~FixedArray();

			Memory::Copy<T>(m_arr, a_rhs.m_arr, arr_size);

			return *this;
		}

        const T& operator[](const size_t a_index) const
        {
            BB_ASSERT(a_index <= arr_size, "FixedArray, trying to access a index that is out of bounds.");
            return m_arr[a_index];
        }

		T& operator[](const size_t a_index)
		{
			BB_ASSERT(a_index <= arr_size, "FixedArray, trying to access a index that is out of bounds.");
			return m_arr[a_index];
		}

		const ConstSlice<T> const_slice() const
		{
			return const_slice(arr_size);
		}

		const ConstSlice<T> const_slice(const size_t a_size, const size_t a_begin = 0) const
		{
			BB_ASSERT(a_begin + a_size <= arr_size, "requesting an out of bounds slice");
			return ConstSlice<T>(&m_arr[a_begin], a_size);
		}

		const Slice<T> slice()
		{
			return slice(arr_size);
		}

		const Slice<T> slice(const size_t a_size, const size_t a_begin = 0)
		{
			BB_ASSERT(a_begin + a_size <= arr_size, "requesting an out of bounds slice");
			return Slice<T>(&m_arr[a_begin], a_size);
		}

		constexpr size_t size() const { return arr_size; }
		constexpr const T* data() const { return m_arr; }

	private:
		T m_arr[arr_size]{};
	};
}
