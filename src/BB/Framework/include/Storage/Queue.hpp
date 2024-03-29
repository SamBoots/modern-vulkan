#pragma once
#include "Utils/Logger.h"
#include "MemoryArena.hpp"

namespace BB
{
	template<typename T>
	class Queue
	{
	public:
		void Init(MemoryArena& a_arena, const size_t a_element_count)
		{
			m_begin = ArenaAllocArr(a_arena, T, a_element_count);
			m_end = m_begin + a_element_count;
			m_front_queue = -1u;
			m_back_queue = 0;

#ifdef _DEBUG
			m_size = 0;
#endif // _DEBUG
		}

		void EnQueue(T& a_element)
		{
			BB_ASSERT(!IsFull(), "trying to add a queue element while the queue is full");

			// if we are empty then set all elements back to the beginning of the memory
			if (IsEmpty())
			{
				m_front_queue = 0;
				m_back_queue = 0;
			}

			m_begin[m_back_queue] = a_element;

			if (&m_begin[++m_back_queue] == m_end)
				m_back_queue = 0;

#ifdef _DEBUG
			++m_size;
#endif // _DEBUG
		}

		void DeQueue()
		{
			BB_ASSERT(!IsEmpty(), "trying to remove a queue element while the queue is empty");

			if (&m_begin[++m_front_queue] == m_end)
				m_front_queue = 0;

			if (m_front_queue == m_back_queue)
			{
				m_front_queue = -1u;
			}

#ifdef _DEBUG
			--m_size;
#endif // _DEBUG
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
#ifdef _DEBUG
		uint32_t m_size;
#endif // _DEBUG
	};
}
