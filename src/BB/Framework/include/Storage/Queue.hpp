#pragma once
#include "Utils/Logger.h"
#include "MemoryArena.hpp"
#

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
			m_front_queue = 0;
			m_back_queue = 0;

#ifdef _DEBUG
			m_size = 0;
#endif // _DEBUG
		}

		// thread safe
		void EnQueue(T& a_element)
		{
			BB_ASSERT(!IsFull(), "trying to add a queue element while the queue is full");

			const uint32_t head = BBInterlockedIncrement32(&m_back_queue);
			InterlockedExchange
			m_begin[head] = a_element;


			if (&m_begin[head] == m_end)
				m_back_queue = 0;

#ifdef _DEBUG
			++m_size;
#endif // _DEBUG
		}
		
		// thread unsafe
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
		std::atomic<T>* m_begin;
		T* m_end;
		uint32_t m_front_queue;
		std::atomic<uint32_t> m_back_queue;
#ifdef _DEBUG
		uint32_t m_size;
#endif // _DEBUG
	};
}
