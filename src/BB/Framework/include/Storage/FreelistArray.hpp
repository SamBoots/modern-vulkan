#pragma once
#include "Utils/Utils.h"
#include "BBMemory.h"
#include "Slice.h"

#include "Common.h"

#include "MemoryArena.hpp"

namespace BB
{
	template <typename T>
	class FreelistArray
	{
		BB_STATIC_ASSERT(std::is_trivially_destructible_v<T>, "Freelist array cannot destruct elements on clear");
		BB_STATIC_ASSERT(sizeof(T) >= sizeof(size_t), "T must be equal or larger then size_t");
		union ValueIndex
		{
			size_t index;
			T value;
		};

	public:
		FreelistArray()
		{
			m_arr = nullptr;

			m_capacity = 0;
			m_size = 0;
			//index to m_id_arr
			m_next_free = 0;
		}
		FreelistArray(const FreelistArray& a_map) = delete;
		FreelistArray(FreelistArray&& a_map) = delete;

		FreelistArray& operator=(const FreelistArray& a_rhs) = delete;
		FreelistArray& operator=(FreelistArray&& a_rhs) = delete;

		void Init(MemoryArena& a_arena, const uint32_t a_size)
		{
			BB_ASSERT(m_arr == nullptr, "initializing a static slotmap while it was already initialized or not set to 0");
			m_capacity = a_size;

			m_arr = ArenaAllocArr(a_arena, ValueIndex, m_capacity);
			clear();
		}

		void Destroy()
		{
			Memory::Set(this, 0, 1);
		}

		T& operator[](const size_t a_handle) const
		{
			return find(a_handle);
		}

		size_t insert(T& a_obj)
		{
			return emplace(a_obj);
		}

		template <class... Args>
		size_t emplace(Args&&... a_args)
		{
			BB_ASSERT(m_size < m_capacity, "static freelistarray over capacity!");
			const size_t index = m_next_free;
			m_next_free = m_arr[index].index;

			new (&m_arr[index].value) T(std::forward<Args>(a_args)...);
			++m_size;
			return index;
		}

		T& find(const size_t a_index) const
		{
			return m_arr[a_index].value;
		}

		void erase(const size_t a_index)
		{
			m_arr[a_index].index = m_next_free;
			m_next_free = a_index;
		}

		void clear()
		{
			m_size = 0;

			for (uint32_t i = 0; i < m_capacity; ++i)
			{
				m_arr[i].index = i + 1;
			}
			m_next_free = 0;
		}

		uint32_t size() const { return m_size; }
		uint32_t capacity() const { return m_capacity; }

	private:
		ValueIndex* m_arr;

		uint32_t m_capacity;
		uint32_t m_size = 0;
		//index to m_id_arr
		size_t m_next_free;
	};
}
