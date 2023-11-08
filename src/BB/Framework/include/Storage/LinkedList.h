#pragma once
#include "Utils/Logger.h"

#include <type_traits>

namespace BB
{
	template<typename T>
	struct LinkedListNode
	{
		T* next;
	};

	template<typename T>
	class LinkedList
	{
		static_assert(std::is_base_of<LinkedListNode<T>, T>::value, "T is not derived from LinkedListNode");
	public:
		LinkedList()
		{
			head = nullptr;
		}
		explicit LinkedList(T* a_head)
		{
			head = a_head;
		}

		LinkedList<T> operator=(T* a_head)
		{
			head = a_head;
			return *this;
		}

		void Push(T* a_next)
		{
			a_next->next = head;
			head = a_next;
		}

		T* Pop()
		{
			BB_ASSERT(head != nullptr, "Linked List is empty!");
			T* ret_v = head;
			head = head->next;
			return ret_v;
		}

		void MergeList(LinkedList<T>& a_list)
		{
			T* list_end = a_list.head;
			while (list_end->next != nullptr)
				list_end = list_end->next;
			list_end->next = head;
			head = a_list.head;
		}

		bool HasEntry() const
		{
			return head;
		}

		T* head;
	};
}