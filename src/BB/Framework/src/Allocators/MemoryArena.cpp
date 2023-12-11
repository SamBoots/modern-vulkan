#include "MemoryArena.hpp"
#include "Program.h"

using namespace BB;

static inline bool PointerWithinArena(const MemoryArena& a_arena, const void* a_pointer)
{
	if (a_pointer > a_arena.buffer && a_pointer < a_arena.end)
		return true;

	return false;
}

static inline size_t GetAddressRange(const void* a_begin, const void* a_end)
{
	return reinterpret_cast<uintptr_t>(a_end) - reinterpret_cast<uintptr_t>(a_begin);
}

static inline void ChangeArenaAt(MemoryArena& a_arena, void* a_at)
{
	BB_ASSERT(PointerWithinArena(a_arena, a_at), "modifying memory arena at that is not inside the arena, arena may be full");
	if (a_at > a_arena.commited)
	{
		const size_t commit_range = Max(ARENA_DEFAULT_COMMIT, GetAddressRange(a_arena.commited, a_at));
		BB_ASSERT(CommitVirtualMemory(a_arena.commited, commit_range), "increase commit range of memory arena failed");

		a_arena.commited = Pointer::Add(a_arena.commited, commit_range);
	}

	a_arena.at = a_at;
}

MemoryArena BB::MemoryArenaCreate(const size_t a_reserve_size)
{
	MemoryArena memory_arena;
	memory_arena.buffer = ReserveVirtualMemory(a_reserve_size);
	memory_arena.commited = memory_arena.buffer;
	memory_arena.end = Pointer::Add(memory_arena.buffer, a_reserve_size);
	memory_arena.at = memory_arena.buffer;

	memory_arena.owns_memory = true;
	return memory_arena;
}

MemoryArena BB::MemoryArenaCreate(MemoryArena& a_memory_source, const size_t a_memory_size)
{
	MemoryArena memory_arena;
	memory_arena.buffer = MemoryArenaAlloc(a_memory_source, a_memory_size, 8);
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
	memset(a_arena.buffer, 0, GetAddressRange(a_arena.buffer, a_arena.at));
	a_arena.at = a_arena.buffer;
}

void* BB::MemoryArenaAlloc_f(BB_ARENA_DEBUG MemoryArena& a_arena, const size_t a_memory_size, const size_t a_align)
{
	void* return_address = a_arena.at;

	void* new_at = Pointer::Add(a_arena.at, a_memory_size + Pointer::AlignForwardAdjustment(a_arena.at, a_align));
	ChangeArenaAt(a_arena, new_at);

	// do debug stuff here

	return return_address;
}
