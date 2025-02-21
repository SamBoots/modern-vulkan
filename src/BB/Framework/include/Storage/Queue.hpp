#pragma once
#include "Utils/Logger.h"
#include "MemoryArena.hpp"
#include <atomic>

namespace BB
{
	template<typename T>
	class SPSCQueue
	{
	public:
		void Init(MemoryArena& a_arena, const size_t a_element_count)
		{
			m_begin = ArenaAllocArr(a_arena, T, a_element_count);
			m_end = m_begin + a_element_count;
			m_front_queue = -1u;
			m_back_queue = 0;
		}

		bool EnQueue(T& a_element)
		{
			if (IsFull())
			{
				BB_WARNING(false, "trying to add a queue element while the queue is full", WarningType::HIGH);
				return false;
			}

			// if we are empty then set all elements back to the beginning of the memory
			if (IsEmpty())
			{
				m_front_queue = 0;
				m_back_queue = 0;
			}

			m_begin[m_back_queue] = a_element;

			if (&m_begin[++m_back_queue] == m_end)
				m_back_queue = 0;
			return true;
		}

		T DeQueue()
		{
			BB_ASSERT(!IsEmpty(), "trying to remove a queue element while the queue is empty");
			const uint32_t front_queue = m_front_queue;
			if (&m_begin[++m_front_queue] == m_end)
				m_front_queue = 0;

			if (m_front_queue == m_back_queue)
			{
				m_front_queue = -1u;
			}

			return m_begin[front_queue];
		}

		inline const T* Peek() const
		{
			if (IsEmpty())
				return nullptr;
			return &m_begin[m_front_queue];
		}

		inline bool IsEmpty() const
		{
			return m_front_queue == -1u;
		}

		inline bool IsFull() const
		{
			return m_front_queue == m_back_queue;
		}

		inline size_t Capacity() const { return reinterpret_cast<size_t>(Pointer::Subtract(m_end, reinterpret_cast<size_t>(m_begin))); }

	private:
		T* m_begin;
		T* m_end;
		uint32_t m_front_queue;
		uint32_t m_back_queue;
	};

	template<typename T>
	class MPSCQueue
	{
	public:
		void Init(MemoryArena& a_arena, const size_t a_element_count)
		{
			m_arr = ArenaAllocArr(a_arena, std::atomic<T>, a_element_count);
			m_capacity = a_element_count;
			m_tail = 0;
			m_head = 0;
			m_size = 0;
		}

		// thread safe
		bool EnQueue(const T& a_element)
		{
			const size_t current_size = m_size.fetch_add(1, std::memory_order_acquire);
			if (current_size >= m_capacity)
			{
				m_size.fetch_sub(1);
				return false;
			}

			const size_t head = m_head.fetch_add(1, std::memory_order_acquire) % m_capacity;
			m_arr[head].exchange(a_element, std::memory_order_release);
			return true;
		}

		// thread unsafe
		bool DeQueue(T& a_out)
		{
			if (m_size == 0)
			{
				return false;
			}
			a_out = m_arr[m_tail].load();
			m_size.fetch_sub(1, std::memory_order_release);
			if (++m_tail >= m_capacity)
				m_tail = 0;

			return true;
		}

		inline bool IsEmpty() const
		{
			return !m_size;
		}

		inline bool IsFull() const
		{
			return m_capacity == m_size;
		}

		inline size_t Capacity() const { return m_capacity; }

	private:
		std::atomic<T>* m_arr;
		size_t m_capacity;
		std::atomic<uint32_t> m_size;
		uint32_t m_tail;
		std::atomic<size_t> m_head;
	};
}
