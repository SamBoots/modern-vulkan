#pragma once
#include "Allocators/Allocators.h"
#include "Utils/Utils.h"
#include "Utils/Logger.h"
#include <malloc.h>
#include <type_traits>

////incase BBnewArr / BBStackAlloc / BBStackAlloc_s do not work anymore due to template stuff, use this again.
////This was required once to make templates work in macros but not anymore?
//template <typename T>
//struct MacroType { typedef T type; }; //I hate C++.

namespace BB
{
//_alloca wrapper, does not require a free call.
#define BBstackAlloc(a_count, a_type) reinterpret_cast<a_type*>(_alloca(a_count * sizeof(a_type)))
//_malloca wrapper, be sure to call BBstackFree_s
#define BBstackAlloc_s(a_count, a_type) reinterpret_cast<a_type*>(_malloca(a_count * sizeof(a_type)))
#define BBstackFree_s(a_ptr) _freea(a_ptr)

#define BBalloc(a_allocator, a_size) BB::BBalloc_f(BB_MEMORY_DEBUG_ARGS a_allocator, a_size, sizeof(size_t))
#define BBnew(a_allocator, a_type) new (BB::BBalloc_f(BB_MEMORY_DEBUG_ARGS a_allocator, sizeof(a_type), __alignof(a_type))) a_type
#define BBnewArr(a_allocator, a_length, a_type) (BB::BBnewArr_f<a_type>(BB_MEMORY_DEBUG_ARGS_ARR a_allocator, a_length))

#define BBfree(a_allocator, a_ptr) BBfree_f(a_allocator, a_ptr)
#define BBfreeArr(a_allocator, a_ptr) BBfreeArr_f(a_allocator, a_ptr)

#define BBmemZero(a_ptr, a_size) memset(a_ptr, 0, a_size)

#define BBStackAllocatorScope(a_stack_allocator) \
	for (auto stack_marker = a_stack_allocator.GetMarker(); stack_marker; a_stack_allocator.SetMarker(stack_marker), stack_marker = 0)

#pragma region AllocationFunctions
	//Use the BBnew or BBalloc function instead of this.
	inline void* BBalloc_f(BB_MEMORY_DEBUG Allocator a_allocator, const size_t a_size, const size_t a_alignment)
	{
		BB_MEMORY_DEBUG_VOID_ARRAY;
		return a_allocator.func(BB_MEMORY_DEBUG_SEND a_allocator.allocator, a_size, a_alignment, nullptr);
	}

	//Use the BBnewArr function instead of this.
	template <typename T>
	inline T* BBnewArr_f(BB_MEMORY_DEBUG Allocator a_allocator, size_t a_length)
	{
		BB_MEMORY_DEBUG_VOID_ARRAY;
		BB_ASSERT(a_length != 0, "Trying to allocate an array with a length of 0.");

		if constexpr (std::is_trivially_constructible_v<T> && std::is_trivially_destructible_v<T>)
		{
			return reinterpret_cast<T*>(a_allocator.func(BB_MEMORY_DEBUG_SEND_ARR a_allocator.allocator, sizeof(T) * a_length, __alignof(T), nullptr));
		}
		else
		{
			//Allocate the array, but shift it by sizeof(size_t) bytes forward to allow the size of the header to be put in as well.
			T* ptr = (reinterpret_cast<T*>(Pointer::Add(a_allocator.func(BB_MEMORY_DEBUG_SEND_ARR a_allocator.allocator, (sizeof(T) * a_length) + sizeof(size_t), __alignof(T), nullptr), sizeof(size_t))));

			//Store the size of the array inside the first element of the pointer.
			*(reinterpret_cast<size_t*>(ptr) - 1) = a_length;

			if constexpr (!std::is_trivially_constructible_v<T>)
			{
				//Create the elements.
				for (size_t i = 0; i < a_length; i++)
					new (&ptr[i]) T();
			}

			return ptr;
		}
	}

	template <typename T>
	inline void BBfree_f(Allocator a_allocator, T* a_ptr)
	{
		BB_ASSERT(a_ptr != nullptr, "Trying to free a nullptr");
		if constexpr (!std::is_trivially_destructible<T>::value)
		{
			BB_WARNINGS_OFF // turn off warnings here due to CLANG thinking this will destruct a type void, which it won't due to is_trivially_destructible
			a_ptr->~T();
			BB_WARNINGS_ON
		}
		a_allocator.func(BB_MEMORY_DEBUG_FREE a_allocator.allocator, 0, 0, a_ptr);
	}

	template <typename T>
	inline void BBfreeArr_f(Allocator a_allocator, T* a_ptr)
	{
		BB_ASSERT(a_ptr != nullptr, "Trying to freeArray a nullptr");

		if constexpr (std::is_trivially_constructible_v<T> || std::is_trivially_destructible_v<T>)
		{
			a_allocator.func(BB_MEMORY_DEBUG_FREE_ARR a_allocator.allocator, 0, 0, a_ptr);
		}
		else
		{
			//get the array size
			size_t length = *(reinterpret_cast<size_t*>(a_ptr) - 1);

			for (size_t i = 0; i < length; i++)
				a_ptr[i].~T();

			size_t header_size;
			if constexpr (sizeof(size_t) % sizeof(T) > 0)
				header_size = sizeof(size_t) / sizeof(T) + 1;
			else
				header_size = sizeof(size_t) / sizeof(T);

			a_allocator.func(BB_MEMORY_DEBUG_FREE_ARR a_allocator.allocator, 0, 0, a_ptr - header_size);
		}
	}

	//if a_tag_name is nullptr then it resets the tag name.
	template <typename T>
	inline void BBTagAlloc(const T* a_ptr, const char* a_tag_name)
	{
		typedef allocators::BaseAllocator::AllocationLog AllocationLog;
		AllocationLog* log = reinterpret_cast<AllocationLog*>(Pointer::Subtract(a_ptr, sizeof(AllocationLog)));
		//do a check to see if the boundries are there, if yes. Then it's a 99.99999% chance this is a existing allocation using BB.
		uintptr_t back = reinterpret_cast<uintptr_t>(log->back) + MEMORY_BOUNDRY_FRONT;
		uintptr_t front = reinterpret_cast<uintptr_t>(log->front);
		if ((back - front) != log->alloc_size)
		{
			//try again, this time we may have a array.
			//SPECIAL NOTE: I hate this code, tagging is a not-essential debug tool anyway but goddamn wtf did I make here.


			log = reinterpret_cast<AllocationLog*>(Pointer::Subtract(log, sizeof(size_t)));
			back = reinterpret_cast<uintptr_t>(log->back) + MEMORY_BOUNDRY_FRONT;
			front = reinterpret_cast<uintptr_t>(log->front);

			BB_ASSERT(((back - front) == log->alloc_size), "BBTagAlloc is not tagging a memory space allocated by a BB allocator");
			BB_ASSERT(log->is_array, "BBTagAlloc tries to correct for an array write but the memory is not an array!");
		}

		log->tagName = a_tag_name;
	}
}
#pragma endregion // AllocationFunctions
