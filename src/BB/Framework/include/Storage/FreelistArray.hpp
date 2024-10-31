#pragma once
#include "Utils/Utils.h"
#include "BBMemory.h"
#include "Slice.h"

#include "Common.h"

#include "MemoryArena.hpp"

using namespace BB
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
		FreelistArray(const FreelistArray<T>& a_map) = delete;
		FreelistArray(FreelistArray<T>&& a_map) = delete;

		FreelistArray<T>& operator=(const FreelistArray<T>& a_rhs) = delete;
		FreelistArray<T>& operator=(FreelistArray<T>&& a_rhs) = delete;

		void Init(MemoryArena& a_arena, const uint32_t a_size)
		{
			BB_ASSERT(m_id_arr == nullptr, "initializing a static slotmap while it was already initialized or not set to 0");
			m_capacity = a_size;

			m_arr = ArenaAllocArr(a_arena, ValueIndex, m_capacity);

			for (uint32_t i = 0; i < m_capacity - 1; ++i)
			{
				m_arr[i].index = i + 1;
			}

			m_next_free = 0;
		}

		void Destroy()
		{
			Memory::Set(this, 0, 1);
		}

		T& operator[](const HandleType a_handle) const
		{
			CheckGen(a_handle);
			return find(a_handle);
		}

		HandleType insert(T& a_obj)
		{
			return emplace(a_obj);
		}

		template <class... Args>
		uint32_t emplace(Args&&... a_args)
		{
			BB_ASSERT(m_size < m_capacity, "static slotmap over capacity!");

			HandleType id = m_id_arr[m_next_free];
			id.index = m_next_free;
			//Set the next free to the one that is next, an unused m_id_arr entry holds the next free one.
			m_next_free = m_id_arr[id.index].index;
			m_id_arr[id.index].index = static_cast<uint32_t>(m_size);

			new (&m_obj_arr[m_size]) T(std::forward<Args>(a_args)...);
			m_erase_arr[m_size++] = id.index;

			return id;
		}

		T& find(const uint32_t a_index) const
		{
			return m_arr[a_index];
		}

		void erase(const HandleType a_handle)
		{
			CheckGen(a_handle);
			const uint32_t index = m_id_arr[a_handle.index].index;

			if constexpr (!trivialDestructible_T)
			{
				//Before move call the destructor if it has one.
				m_obj_arr[index].~T();
			}

			m_obj_arr[index] = std::move(m_obj_arr[--m_size]);
			//Increment the gen for when placing it inside the m_id_arr again.
			m_id_arr[m_erase_arr[index]] = a_handle;
			++m_id_arr[m_erase_arr[index]].extra_index;
			m_erase_arr[index] = std::move(m_erase_arr[m_size]);
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
		T* data() const { return &m_arr.value[0]; }

	private:
		ValueIndex* m_arr;

		uint32_t m_capacity;
		uint32_t m_size = 0;
		//index to m_id_arr
		size_t m_next_free;
	};
}
