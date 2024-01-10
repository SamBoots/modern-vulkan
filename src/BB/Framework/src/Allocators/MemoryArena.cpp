#include "MemoryArena.hpp"
#include "Program.h"


#if __has_feature(address_sanitizer) && defined(_DEBUG_POISON_MEMORY_BOUNDRY)
#define SANITIZER_ENABLED
constexpr size_t MEMORY_BOUNDRY_SIZE = 8;
#include <sanitizer/asan_interface.h>
#endif // _DEBUG_POISON_MEMORY_BOUNDRY

using namespace BB;

static inline size_t GetAddressRange(const void* a_begin, const void* a_end)
{
	return reinterpret_cast<uintptr_t>(a_end) - reinterpret_cast<uintptr_t>(a_begin);
}

static inline void ChangeArenaAt(MemoryArena& a_arena, void* a_at)
{
	BB_ASSERT(MemoryArenaIsPointerWithinArena(a_arena, a_at), "modifying memory arena at that is not inside the arena, arena may be full");
	if (a_at > a_arena.commited)
	{
		const size_t commit_range = RoundUp(GetAddressRange(a_arena.commited, a_at), ARENA_DEFAULT_COMMIT);
		BB_ASSERT(CommitVirtualMemory(a_arena.commited, commit_range), "increase commit range of memory arena failed");

		a_arena.commited = Pointer::Add(a_arena.commited, commit_range);
	}

	a_arena.at = a_at;
}

MemoryArena BB::MemoryArenaCreate(const size_t a_reserve_size)
{
	MemoryArena memory_arena{};
	memory_arena.buffer = ReserveVirtualMemory(a_reserve_size);
	memory_arena.commited = memory_arena.buffer;
	memory_arena.end = Pointer::Add(memory_arena.buffer, a_reserve_size);
	memory_arena.at = memory_arena.buffer;

	memory_arena.owns_memory = true;
	return memory_arena;
}

MemoryArena BB::MemoryArenaCreate(MemoryArena& a_memory_source, const size_t a_memory_size)
{
	MemoryArena memory_arena{};
	memory_arena.buffer = ArenaAlloc(a_memory_source, a_memory_size, 8);
	memory_arena.commited = Pointer::Add(memory_arena.buffer, a_memory_size);
	memory_arena.end = Pointer::Add(memory_arena.buffer, a_memory_size);
	memory_arena.at = memory_arena.buffer;

	memory_arena.owns_memory = false;
	return memory_arena;
}

void BB::MemoryArenaFree(MemoryArena& a_arena)
{
	if (a_arena.owns_memory)
	{
		BB_ASSERT(ReleaseVirtualMemory(a_arena.buffer), "failed to release memory");
	}
	else
	{
		MemoryArenaReset(a_arena);
	}

	a_arena.buffer = nullptr;
	a_arena.commited = nullptr;
	a_arena.end = nullptr;
	a_arena.at = nullptr;

	a_arena.owns_memory = false;
}

void BB::MemoryArenaReset(MemoryArena& a_arena)
{
	MemoryArenaDecommitExess(a_arena);
	memset(a_arena.buffer, 0, GetAddressRange(a_arena.buffer, a_arena.commited));
	a_arena.at = a_arena.buffer;
}

void BB::MemoryArenaDecommitExess(MemoryArena& a_arena)
{
	if (a_arena.owns_memory)
	{
		const void* const aligned_at = Pointer::AlignAddress(a_arena.at, ARENA_DEFAULT_COMMIT);
		void* const decommit_start = Max(aligned_at, Pointer::Add(a_arena.buffer, ARENA_DEFAULT_COMMITTED_SIZE));
		const size_t decommit_size = Max(0u, GetAddressRange(decommit_start, a_arena.commited));

		if (decommit_size > 0)
		{
			BB_ASSERT(DecommitVirtualMemory(decommit_start, decommit_size), "decommiting memory arena failed");
			a_arena.commited = decommit_start;
		}
	}
}

void BB::TagMemory(const MemoryArena& a_arena, void* a_ptr, const char* a_tag_name)
{
	BB_ASSERT(MemoryArenaIsPointerWithinArena(a_arena, a_ptr), "Trying to tag memory that is not within the memory arena!");
#ifdef SANITIZER_ENABLED
	constexpr size_t SUBTRACT_VALUE = sizeof(MemoryArenaAllocationInfo) + MEMORY_BOUNDRY_SIZE;
#else
	constexpr size_t SUBTRACT_VALUE = sizeof(MemoryArenaAllocationInfo);
#endif // SANITIZER_ENABLED

	MemoryArenaAllocationInfo* allocation_info = reinterpret_cast<MemoryArenaAllocationInfo*>(Pointer::Subtract(a_ptr, SUBTRACT_VALUE));
	allocation_info->tag_name = a_tag_name;
}

static void MemoryArenaResetTo(MemoryArena& a_arena, void* a_memory_marker)
{
#ifdef _DEBUG_MEMORY
	while (a_arena.last)
	{
		if (a_arena.last->alloc_address <= a_memory_marker)
			break;

		a_arena.last = a_arena.last->prev;
	}
#endif // _DEBUG_MEMORY

#ifdef SANITIZER_ENABLED
	// memory region could be poisoned, remove the poison.
	const size_t a_range = GetAddressRange(a_arena.at, a_memory_marker.at);
	__asan_unpoison_memory_region(a_memory_marker.at, a_range);
#endif // SANITIZER_ENABLED

	a_arena.at = a_memory_marker;
	MemoryArenaDecommitExess(a_arena);
}

MemoryArenaMarker BB::MemoryArenaGetMemoryMarker(const MemoryArena& a_arena)
{
	return MemoryArenaMarker{ a_arena, a_arena.at };
}

void BB::MemoryArenaSetMemoryMarker(MemoryArena& a_arena, const MemoryArenaMarker& a_memory_marker)
{
	BB_ASSERT(a_arena.buffer == a_memory_marker.owner.buffer, "not the same allocator");
	BB_ASSERT(MemoryArenaIsPointerWithinArena(a_arena, a_memory_marker.at), "MemoryArenaMarker.at not within memory arena");

	MemoryArenaResetTo(a_arena, a_memory_marker.at);
}

const MemoryArenaAllocationInfo* BB::MemoryArenaGetFrontAllocationLog(const MemoryArena& a_arena)
{
	return a_arena.first;
}

size_t BB::MemoryArenaSizeRemaining(const MemoryArena& a_arena)
{
	return GetAddressRange(a_arena.at, a_arena.end);
}

size_t BB::MemoryArenaSizeCommited(const MemoryArena& a_arena)
{
	return GetAddressRange(a_arena.buffer, a_arena.commited);
}

size_t BB::MemoryArenaSizeUsed(const MemoryArena& a_arena)
{
	return GetAddressRange(a_arena.buffer, a_arena.at);
}

bool BB::MemoryArenaIsPointerWithinArena(const MemoryArena& a_arena, const void* a_pointer)
{
	if (a_pointer > a_arena.buffer && a_pointer < a_arena.end)
		return true;

	return false;
}

void* BB::ArenaAlloc_f(BB_ARENA_DEBUG MemoryArena& a_arena, size_t a_memory_size, const uint32_t a_align)
{
	void* ret_add = ArenaAllocNoZero_f(BB_ARENA_DEBUG_SEND a_arena, a_memory_size, a_align);
	memset(ret_add, 0, a_memory_size);
	return ret_add;
}

void* BB::ArenaAllocNoZero_f(BB_ARENA_DEBUG MemoryArena& a_arena, size_t a_memory_size, const uint32_t a_align)
{
#ifdef _DEBUG_MEMORY
	// do debug stuff here
	MemoryArenaAllocationInfo* debug_address = reinterpret_cast<MemoryArenaAllocationInfo*>(a_arena.at);
	ChangeArenaAt(a_arena, Pointer::Add(a_arena.at, sizeof(MemoryArenaAllocationInfo)));
	memset(debug_address, 0, sizeof(MemoryArenaAllocationInfo));

	debug_address->alloc_address = Pointer::AlignAddress(a_arena.at, a_align);
	debug_address->file = a_file;
	debug_address->line = a_line;
	debug_address->alloc_size = a_memory_size;
	debug_address->alignment = a_align;
	debug_address->tag_name = nullptr;
	debug_address->next = nullptr;
	debug_address->prev = a_arena.last;

	if (a_arena.first == nullptr)
	{
		a_arena.first = debug_address;
		a_arena.last = debug_address;
	}
	else
	{
		a_arena.last->next = debug_address;
		a_arena.last = debug_address;
	}

#endif // _DEBUG_MEMORY
	
	void* return_address = Pointer::AlignAddress(a_arena.at, a_align);

#ifdef SANITIZER_ENABLED
	a_memory_size += MEMORY_BOUNDRY_SIZE * 2;
	void* return_address = Pointer::Add(return_address, MEMORY_BOUNDRY_SIZE);
#endif // SANITIZER_ENABLED

	void* new_at = Pointer::Add(return_address, a_memory_size);
	ChangeArenaAt(a_arena, new_at);

#ifdef SANITIZER_ENABLED
	__asan_poison_memory_region(Pointer::Subtract(return_address, MEMORY_BOUNDRY_SIZE), MEMORY_BOUNDRY_SIZE);
	__asan_poison_memory_region(Pointer::Add(return_address, a_memory_size - MEMORY_BOUNDRY_SIZE), MEMORY_BOUNDRY_SIZE);
#endif // SANITIZER_ENABLED

	return return_address;
}
