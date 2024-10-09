#pragma once
#include "Utils/Utils.h"
#include "BBMemory.h"
#include "Slice.h"

#include "Common.h"

#include "MemoryArena.hpp"

namespace BB
{
	namespace Slotmap_Specs
	{
		constexpr const size_t multipleValue = 8;
		constexpr const size_t standardSize = 8;
	}

	//default slotmap handle
	//first 32 bytes is the Index
	//second 32 bytes is the generation
	//very flaky as it's a replica of FrameworkHandle, but I want this to not be explicit
	union SlotmapHandle
	{
		SlotmapHandle() { handle = 0; }
		SlotmapHandle(const uint64_t a_handle) : handle(a_handle) {}
		uint64_t handle;
		struct
		{
			uint32_t index;
			uint32_t extra_index;
		};
	};

	template <typename T, typename HandleType = SlotmapHandle>
	class Slotmap
	{
		static constexpr bool trivialDestructible_T = std::is_trivially_destructible_v<T>;

	public:
		struct Iterator
		{
			Iterator(T* a_ptr) : m_ptr(a_ptr) {}

			T& operator*() const { return *m_ptr; }
			T* operator->() { return m_ptr; }

			Iterator& operator++()
			{
				++m_ptr;
				return *this;
			}

			Iterator operator++(int)
			{
				Iterator t_Tmp = *this;
				++(*this);
				return t_Tmp;
			}

			friend bool operator< (const Iterator& a_lhs, const Iterator& a_rhs) { return a_lhs.m_ptr < a_rhs.m_ptr; }
			friend bool operator> (const Iterator& a_lhs, const Iterator& a_rhs) { return a_lhs.m_ptr > a_rhs.m_ptr; }
			friend bool operator<= (const Iterator& a_lhs, const Iterator& a_rhs) { return a_lhs.m_ptr <= a_rhs.m_ptr; }
			friend bool operator>= (const Iterator& a_lhs, const Iterator& a_rhs) { return a_lhs.m_ptr >= a_rhs.m_ptr; }

		private:
			T* m_ptr;
		};

		Slotmap(Allocator a_allocator)
			: Slotmap(a_allocator, Slotmap_Specs::standardSize)
		{}
		Slotmap(Allocator a_allocator, const uint32_t a_size)
		{
			m_allocator = a_allocator;
			m_capacity = a_size;

			m_id_arr = reinterpret_cast<HandleType*>(BBalloc(m_allocator, (sizeof(HandleType) + sizeof(T) + sizeof(uint32_t)) * m_capacity));
			m_obj_arr = reinterpret_cast<T*>(Pointer::Add(m_id_arr, sizeof(HandleType) * m_capacity));
			m_erase_arr = reinterpret_cast<uint32_t*>(Pointer::Add(m_obj_arr, sizeof(T) * m_capacity));

			for (uint32_t i = 0; i < m_capacity - 1; ++i)
			{
				m_id_arr[i].index = i + 1;
				m_id_arr[i].extra_index = 1;
			}
			m_id_arr[m_capacity - 1].extra_index = 1;
			m_next_free = 0;
		}
		Slotmap(const Slotmap<T>& a_map)
		{
			m_allocator = a_map.m_allocator;
			m_capacity = a_map.m_capacity;
			m_size = a_map.m_size;
			m_next_free = a_map.m_next_free;

			m_id_arr = reinterpret_cast<HandleType*>(BBalloc(m_allocator, (sizeof(HandleType) + sizeof(T) + sizeof(uint32_t)) * m_capacity));
			m_obj_arr = reinterpret_cast<T*>(Pointer::Add(m_id_arr, sizeof(HandleType) * m_capacity));
			m_erase_arr = reinterpret_cast<uint32_t*>(Pointer::Add(m_obj_arr, sizeof(T) * m_capacity));

			BB::Memory::Copy(m_id_arr, a_map.m_id_arr, m_capacity);
			BB::Memory::Copy(m_obj_arr, a_map.m_obj_arr, m_size);
			BB::Memory::Copy(m_erase_arr, a_map.m_erase_arr, m_size);
		}
		Slotmap(Slotmap<T>&& a_map) noexcept
		{
			m_capacity = a_map.m_capacity;
			m_size = a_map.m_size;
			m_next_free = a_map.m_next_free;
			m_id_arr = a_map.m_id_arr;
			m_obj_arr = a_map.m_obj_arr;
			m_erase_arr = a_map.m_erase_arr;
			m_allocator = a_map.m_allocator;

			a_map.m_capacity = 0;
			a_map.m_size = 0;
			a_map.m_next_free = 1;
			a_map.m_id_arr = nullptr;
			a_map.m_obj_arr = nullptr;
			a_map.m_erase_arr = nullptr;
			a_map.m_allocator.allocator = nullptr;
			a_map.m_allocator.func = nullptr;
		}
		~Slotmap()
		{
			if (m_id_arr != nullptr)
			{
				if constexpr (!trivialDestructible_T)
				{
					for (uint32_t i = 0; i < m_size; i++)
					{
						m_obj_arr[i].~T();
					}
				}

				BBfree(m_allocator, m_id_arr);
			}
		}

		Slotmap<T>& operator=(const Slotmap<T>& a_rhs)
		{
			this->~Slotmap();

			m_allocator = a_rhs.m_allocator;
			m_capacity = a_rhs.m_capacity;
			m_size = a_rhs.m_size;
			m_next_free = a_rhs.m_next_free;

			m_id_arr = reinterpret_cast<HandleType*>(BBalloc(m_allocator, (sizeof(HandleType) + sizeof(T) + sizeof(uint32_t)) * m_capacity));
			m_obj_arr = reinterpret_cast<T*>(Pointer::Add(m_id_arr, sizeof(HandleType) * m_capacity));
			m_erase_arr = reinterpret_cast<uint32_t*>(Pointer::Add(m_obj_arr, sizeof(T) * m_capacity));

			BB::Memory::Copy(m_id_arr, a_rhs.m_id_arr, m_capacity);
			BB::Memory::Copy(m_obj_arr, a_rhs.m_obj_arr, m_size);
			BB::Memory::Copy(m_erase_arr, a_rhs.m_erase_arr, m_size);

			return *this;
		}
		Slotmap<T>& operator=(Slotmap<T>&& a_rhs) noexcept
		{
			this->~Slotmap();

			m_capacity = a_rhs.m_capacity;
			m_size = a_rhs.m_size;
			m_next_free = a_rhs.m_next_free;
			m_id_arr = a_rhs.m_id_arr;
			m_obj_arr = a_rhs.m_obj_arr;
			m_erase_arr = a_rhs.m_erase_arr;
			m_allocator = a_rhs.m_allocator;

			a_rhs.m_capacity = 0;
			a_rhs.m_size = 0;
			a_rhs.m_next_free = 1;
			a_rhs.m_id_arr = nullptr;
			a_rhs.m_obj_arr = nullptr;
			a_rhs.m_erase_arr = nullptr;;
			a_rhs.m_allocator.allocator = nullptr;
			a_rhs.m_allocator.func = nullptr;

			return *this;
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
		HandleType emplace(Args&&... a_args)
		{
			if (m_size >= m_capacity)
				grow();

			HandleType id = m_id_arr[m_next_free];
			id.index = m_next_free;
			//Set the next free to the one that is next, an unused m_id_arr entry holds the next free one.
			m_next_free = m_id_arr[id.index].index;
			m_id_arr[id.index].index = static_cast<uint32_t>(m_size);

			new (&m_obj_arr[m_size]) T(std::forward<Args>(a_args)...);
			m_erase_arr[m_size++] = id.index;

			return id;
		}
		T& find(const HandleType a_handle) const
		{
			CheckGen(a_handle);
			return m_obj_arr[m_id_arr[a_handle.index].index];
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

		void reserve(const uint32_t a_capacity)
		{
			if (a_capacity > m_capacity)
				reallocate(a_capacity);
		}

		void clear()
		{
			m_size = 0;

			for (uint32_t i = 0; i < m_capacity; ++i)
			{
				m_id_arr[i].index = i + 1;
			}
			m_next_free = 0;

			//Destruct all the variables when it is not trivially destructable.
			if constexpr (!trivialDestructible_T)
			{
				for (uint32_t i = 0; i < m_size; i++)
				{
					m_obj_arr[i].~T();
				}
			}
		}

		Iterator begin() { return Iterator(m_obj_arr); }
		Iterator end() { return Iterator(&m_obj_arr[m_size]); }

		uint32_t size() const { return m_size; }
		uint32_t capacity() const { return m_capacity; }
		T* data() const { return m_obj_arr; }

	private:
		void CheckGen(const HandleType a_handle) const
		{
			BB_ASSERT(m_id_arr[a_handle.index].extra_index == a_handle.extra_index,
				"Slotmap, Handle is from the wrong generation! Likely means this handle was already used to delete an element.");
		}

		void grow()
		{
			reallocate(m_capacity * 2);
		}
		//This function also changes the m_capacity value.
		void reallocate(uint32_t a_new_capacity)
		{
			BB_ASSERT(a_new_capacity < UINT32_MAX, "Slotmap's too big! Slotmaps cannot be bigger then UINT32_MAX");

			HandleType* new_id_arr = reinterpret_cast<HandleType*>(BBalloc(m_allocator, (sizeof(HandleType) + sizeof(T) + sizeof(uint32_t)) * a_new_capacity));
			T* new_obj_arr = reinterpret_cast<T*>(Pointer::Add(new_id_arr, sizeof(HandleType) * a_new_capacity));
			uint32_t* new_erase_arr = reinterpret_cast<uint32_t*>(Pointer::Add(new_obj_arr, sizeof(T) * a_new_capacity));

			BB::Memory::Copy(new_id_arr, m_id_arr, m_capacity);
			BB::Memory::Copy(new_obj_arr, m_obj_arr, m_size);
			BB::Memory::Copy(new_erase_arr, m_erase_arr, m_size);

			for (uint32_t i = m_capacity; i < a_new_capacity - 1; ++i)
			{
				new_id_arr[i].index = static_cast<uint64_t>(i) + 1;
				new_id_arr[i].extra_index = 1;
			}

			m_id_arr[a_new_capacity - 1].extra_index = 1;

			this->~Slotmap();

			m_capacity = a_new_capacity;
			m_id_arr = new_id_arr;
			m_obj_arr = new_obj_arr;
			m_erase_arr = new_erase_arr;
		}

		Allocator m_allocator;

		HandleType* m_id_arr;
		T* m_obj_arr;
		uint32_t* m_erase_arr;

		uint32_t m_capacity;
		uint32_t m_size = 0;
		//index to m_id_arr
		uint32_t m_next_free;
	};


	template <typename T, typename HandleType = SlotmapHandle>
	class StaticSlotmap
	{
		static constexpr bool trivialDestructible_T = std::is_trivially_destructible_v<T>;

	public:
		StaticSlotmap()
		{
			m_id_arr = nullptr;
			m_obj_arr = nullptr;
			m_erase_arr = nullptr;

			m_capacity = 0;
			m_size = 0;
			//index to m_id_arr
			m_next_free = 0;
		}
		StaticSlotmap(const Slotmap<T>& a_map) = delete;
		StaticSlotmap(Slotmap<T>&& a_map) = delete;

		Slotmap<T>& operator=(const Slotmap<T>& a_rhs) = delete;
		Slotmap<T>& operator=(Slotmap<T>&& a_rhs) = delete;

		void Init(MemoryArena& a_arena, const uint32_t a_size)
		{
			BB_ASSERT(m_id_arr == nullptr, "initializing a static slotmap while it was already initialized or not set to 0");
			m_capacity = a_size;

			m_id_arr = reinterpret_cast<HandleType*>(ArenaAlloc(a_arena, (sizeof(HandleType) + sizeof(T) + sizeof(uint32_t)) * m_capacity, 8));
			m_obj_arr = reinterpret_cast<T*>(Pointer::Add(m_id_arr, sizeof(HandleType) * m_capacity));
			m_erase_arr = reinterpret_cast<uint32_t*>(Pointer::Add(m_obj_arr, sizeof(T) * m_capacity));

			for (uint32_t i = 0; i < m_capacity - 1; ++i)
			{
				m_id_arr[i].index = i + 1;
				m_id_arr[i].extra_index = 1;
			}
			m_id_arr[m_capacity - 1].extra_index = 1;
			m_next_free = 0;
		}

		void Init(Allocator a_allocator, const uint32_t a_size)
		{
			BB_ASSERT(m_id_arr == nullptr, "initializing a static slotmap while it was already initialized or not set to 0");
			m_capacity = a_size;

			m_id_arr = reinterpret_cast<HandleType*>(BBalloc(a_allocator, (sizeof(HandleType) + sizeof(T) + sizeof(uint32_t)) * m_capacity));
			m_obj_arr = reinterpret_cast<T*>(Pointer::Add(m_id_arr, sizeof(HandleType) * m_capacity));
			m_erase_arr = reinterpret_cast<uint32_t*>(Pointer::Add(m_obj_arr, sizeof(T) * m_capacity));

			for (uint32_t i = 0; i < m_capacity - 1; ++i)
			{
				m_id_arr[i].index = i + 1;
				m_id_arr[i].extra_index = 1;
			}
			m_id_arr[m_capacity - 1].extra_index = 1;
			m_next_free = 0;
		}

		void Destroy(Allocator a_allocator)
		{
			if (m_id_arr != nullptr)
			{
				if constexpr (!trivialDestructible_T)
				{
					for (uint32_t i = 0; i < m_size; i++)
					{
						m_obj_arr[i].~T();
					}
				}

				BBfree(m_allocator, m_id_arr);
			}
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
		HandleType emplace(Args&&... a_args)
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

		T& find(const HandleType a_handle) const
		{
			CheckGen(a_handle);
			return m_obj_arr[m_id_arr[a_handle.index].index];
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
				m_id_arr[i].index = i + 1;
			}
			m_next_free = 0;

			//Destruct all the variables when it is not trivially destructable.
			if constexpr (!trivialDestructible_T)
			{
				for (uint32_t i = 0; i < m_size; i++)
				{
					m_obj_arr[i].~T();
				}
			}
		}

		Slice<const T> slice()
		{
			return slice(m_size);
		}
		Slice<const T> slice(const size_t a_size, const size_t a_begin = 0)
		{
			BB_ASSERT(a_begin + a_size < m_size, "requesting an out of bounds slice");
			return Slice<const T>(&m_obj_arr[a_begin], a_size);
		}

		uint32_t size() const { return m_size; }
		uint32_t capacity() const { return m_capacity; }
		T* data() const { return m_obj_arr; }

	private:
		void CheckGen(const HandleType a_handle) const
		{
			BB_ASSERT(m_id_arr[a_handle.index].extra_index == a_handle.extra_index,
				"Slotmap, Handle is from the wrong generation! Likely means this handle was already used to delete an element.");
		}

		HandleType* m_id_arr;
		T* m_obj_arr;
		uint32_t* m_erase_arr;

		uint32_t m_capacity;
		uint32_t m_size = 0;
		//index to m_id_arr
		uint32_t m_next_free;
	};
}
