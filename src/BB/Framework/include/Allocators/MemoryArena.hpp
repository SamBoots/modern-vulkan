#pragma once
#include "Common.h"

namespace BB
{

#ifdef _DEBUG
#define BB_ARENA_DEBUG const char* a_file, int a_line,
#define BB_ARENA_DEBUG_UNUSED const char*, int, bool,
#define BB_ARENA_DEBUG_ARGS __FILE__, __LINE__,
#define BB_ARENA_DEBUG_SEND a_file, a_line,
#define BB_ARENA_DEBUG_FREE nullptr, 0
#else //No debug
#define BB_ARENA_DEBUG 
#define BB_ARENA_DEBUG_UNUSED
#define BB_ARENA_DEBUG_ARGS
#define BB_ARENA_DEBUG_SEND
#define BB_ARENA_DEBUG_FREE
#endif //_DEBUG

	constexpr size_t ARENA_DEFAULT_RESERVE(gbSize * 16);
	constexpr size_t ARENA_DEFAULT_COMMIT(kbSize * 16);
	constexpr size_t ARENA_DEFAULT_COMMITTED_SIZE = ARENA_DEFAULT_COMMIT;

	//memory arena using virtual memory
	struct MemoryArena
	{
		void* buffer;	//start
		void* commited;	//commit end
		void* end;		//reserved end
		void* at;

		bool owns_memory;
	};

	MemoryArena MemoryArenaCreate(const size_t a_reserve_size = ARENA_DEFAULT_RESERVE);
	MemoryArena MemoryArenaCreate(MemoryArena& a_memory_source, const size_t a_memory_size);
	void MemoryArenaFree(MemoryArena& a_arena);
	void MemoryArenaReset(MemoryArena& a_arena);

	void MemoryArenaDecommitExess(MemoryArena& a_arena);

	//don't use this unless you know what you are doing, use ArenaAlloc instead
	void* ArenaAlloc_f(BB_ARENA_DEBUG MemoryArena& a_arena, const size_t a_memory_size, const size_t a_align);



	
#define ArenaAlloc(a_arena, a_memory_size, a_align) BB::ArenaAlloc_f(BB_ARENA_DEBUG_ARGS a_arena, a_memory_size, a_align)
	
#define ArenaAllocType(a_arena, a_type) new (BB::ArenaAlloc_f(BB_ARENA_DEBUG_ARGS a_arena, sizeof(a_type), alignof(a_type))) a_type
	
#define ArenaAllocArr(a_arena, a_type, a_count) reinterpret_cast<a_type*>(BB::ArenaAlloc_f(BB_ARENA_DEBUG_ARGS a_arena, sizeof(a_type) * a_count, alignof(a_type)))

}
