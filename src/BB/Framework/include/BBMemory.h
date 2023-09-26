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
	constexpr const size_t kbSize = 1024;
	constexpr const size_t mbSize = kbSize * 1024;
	constexpr const size_t gbSize = mbSize * 1024;

//_alloca wrapper, does not require a free call.
#define BBstackAlloc(a_count, a_type) reinterpret_cast<a_type*>(_alloca(a_count * sizeof(a_type)))
//_malloca wrapper, be sure to call BBstackFree_s
#define BBstackAlloc_s(a_count, a_type) reinterpret_cast<a_type*>(_malloca(a_count * sizeof(a_type)))
#define BBstackFree_s(a_ptr) _freea(a_ptr)

#define BBalloc(a_allocator, a_size) BB::BBalloc_f(BB_MEMORY_DEBUG_ARGS a_allocator, a_size, 1)
#define BBnew(a_allocator, a_type) new (BB::BBalloc_f(BB_MEMORY_DEBUG_ARGS a_allocator, sizeof(a_type), __alignof(a_type))) a_type
#define BBnewArr(a_allocator, a_length, a_type) (BB::BBnewArr_f<a_type>(BB_MEMORY_DEBUG_ARGS a_allocator, a_length))

#define BBfree(a_allocator, a_ptr) BBfree_f(a_allocator, a_ptr)
#define BBfreeArr(a_allocator, a_ptr) BBfreeArr_f(a_allocator, a_ptr)

#define BBmemZero(a_ptr, a_size) memset(a_ptr, 0, a_size)

#define BBStackAllocatorScope(a_stack_allocator) \
	for (auto stack_marker = a_stack_allocator.GetMarker(); stack_marker; a_stack_allocator.SetMarker(stack_marker), stack_marker = 0)

#pragma region AllocationFunctions
	//Use the BBnew or BBalloc function instead of this.
	inline void* BBalloc_f(BB_MEMORY_DEBUG Allocator a_allocator, const size_t a_size, const size_t a_Alignment)
	{
		return a_allocator.func(BB_MEMORY_DEBUG_SEND a_allocator.allocator, a_size, a_Alignment, nullptr);
	}

	//Use the BBnewArr function instead of this.
	template <typename T>
	inline T* BBnewArr_f(BB_MEMORY_DEBUG Allocator a_allocator, size_t a_Length)
	{
		BB_ASSERT(a_Length != 0, "Trying to allocate an array with a length of 0.");

		if constexpr (std::is_trivially_constructible_v<T> && std::is_trivially_destructible_v<T>)
		{
			return reinterpret_cast<T*>(a_allocator.func(BB_MEMORY_DEBUG_SEND a_allocator.allocator, sizeof(T) * a_Length, __alignof(T), nullptr));
		}
		else
		{
			size_t header_size;

			if constexpr (sizeof(size_t) % sizeof(T) > 0)
				header_size = sizeof(size_t) / sizeof(T) + 1;
			else
				header_size = sizeof(size_t) / sizeof(T);

			//Allocate the array, but shift it by sizeof(size_t) bytes forward to allow the size of the header to be put in as well.
			T* ptr = (reinterpret_cast<T*>(a_allocator.func(BB_MEMORY_DEBUG_SEND a_allocator.allocator, sizeof(T) * (a_Length + header_size), __alignof(T), nullptr))) + header_size;

			//Store the size of the array inside the first element of the pointer.
			*(reinterpret_cast<size_t*>(ptr) - 1) = a_Length;

			if constexpr (!std::is_trivially_constructible_v<T>)
			{
				//Create the elements.
				for (size_t i = 0; i < a_Length; i++)
					new (&ptr[i]) T();
			}

			return ptr;
		}
	}

	template <typename T>
	inline void BBfree_f(Allocator a_allocator, T* a_Ptr)
	{
		BB_ASSERT(a_Ptr != nullptr, "Trying to free a nullptr");
		if constexpr (!std::is_trivially_destructible_v<T>)
		{
			a_Ptr->~T();
		}
		a_allocator.func(BB_MEMORY_DEBUG_FREE a_allocator.allocator, 0, 0, a_Ptr);
	}

	template <typename T>
	inline void BBfreeArr_f(Allocator a_allocator, T* a_Ptr)
	{
		BB_ASSERT(a_Ptr != nullptr, "Trying to freeArray a nullptr");

		if constexpr (std::is_trivially_constructible_v<T> || std::is_trivially_destructible_v<T>)
		{
			a_allocator.func(BB_MEMORY_DEBUG_FREE a_allocator.allocator, 0, 0, a_Ptr);
		}
		else
		{
			//get the array size
			size_t t_Length = *(reinterpret_cast<size_t*>(a_Ptr) - 1);

			for (size_t i = 0; i < t_Length; i++)
				a_Ptr[i].~T();

			size_t t_HeaderSize;
			if constexpr (sizeof(size_t) % sizeof(T) > 0)
				t_HeaderSize = sizeof(size_t) / sizeof(T) + 1;
			else
				t_HeaderSize = sizeof(size_t) / sizeof(T);

			a_allocator.func(BB_MEMORY_DEBUG_FREE a_allocator.allocator, 0, 0, a_Ptr - t_HeaderSize);
		}
	}

	inline void BBTagAlloc(Allocator a_allocator, const void* a_Ptr, const char* a_TagName)
	{
		typedef allocators::BaseAllocator::AllocationLog AllocationLog;
		AllocationLog* t_Log = reinterpret_cast<AllocationLog*>(Pointer::Subtract(a_Ptr, sizeof(AllocationLog)));
		//do a check to see if the boundries are there, if yes. Then it's a 99.99999% chance this is a existing allocation using BB.
		//not the same allocator tho, so bad usage of this will still bite you in the ass.
		const uintptr_t back = reinterpret_cast<uintptr_t>(t_Log->back) + MEMORY_BOUNDRY_FRONT;
		const uintptr_t front = reinterpret_cast<uintptr_t>(t_Log->front);
		BB_ASSERT((back - front) == t_Log->allocSize, "BBTagAlloc is not tagging a memory space allocated by a BB allocator");

		t_Log->tagName = a_TagName;
	}
}
#pragma endregion // AllocationFunctions