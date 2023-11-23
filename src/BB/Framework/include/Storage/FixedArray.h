#pragma once
#include "Utils/Logger.h"
namespace BB
{
	template<typename T, size_t arr_size>
	struct FixedArray
	{
		T& operator[](const size_t a_index)
		{
			BB_ASSERT(a_index <= arr_size, "FixedArray, trying to access a index that is out of bounds.");
			return m_arr[a_index];
		}

		constexpr size_t size() const { return arr_size; }
		T* data() { return m_arr; }

		T m_arr[arr_size]{};
	};
}
