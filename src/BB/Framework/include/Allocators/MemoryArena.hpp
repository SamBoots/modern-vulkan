#pragma once
#include "Common.h"

namespace BB
{
#define _DEBUG_MEMORY
#define _DEBUG_POISON_MEMORY_BOUNDRY

#ifdef _DEBUG_MEMORY
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
#endif //_DEBUG_MEMORY

	constexpr size_t ARENA_DEFAULT_RESERVE(gbSize * 16);					//16 gb
	constexpr size_t ARENA_DEFAULT_COMMIT(kbSize * 16);						//16 kb
	constexpr size_t ARENA_DEFAULT_COMMITTED_SIZE = ARENA_DEFAULT_COMMIT;	//16 kb

	struct MemoryArenaAllocationInfo
	{
		MemoryArenaAllocationInfo* prev;//8
		MemoryArenaAllocationInfo* next;//16

		void* alloc_address;			//24
		const char* file;				//32
		int line;						//36
		uint32_t alignment;				//40
		size_t alloc_size;				//48
		const char* tag_name;			//56
	};

	struct MemoryArena
	{
		void* buffer;	//start
		void* commited;	//commit end
		void* end;		//reserved end
		void* at;

		bool owns_memory;

#ifdef _DEBUG_MEMORY
		MemoryArenaAllocationInfo* first;
		MemoryArenaAllocationInfo* last;
#endif // _DEBUG_MEMORY
	};

	struct MemoryArenaMarker
	{
		const MemoryArena* owner;
		void* at;
	};

	class MemoryArenaTemp
	{
	public:
		MemoryArenaTemp(MemoryArena& a_arena);
		~MemoryArenaTemp();

		MemoryArenaTemp(MemoryArenaTemp& a_temp_arena);
		operator MemoryArena&() const;

	private:
		MemoryArena& m_arena;
		void* m_at;
	};

	MemoryArena MemoryArenaCreate(const size_t a_reserve_size = ARENA_DEFAULT_RESERVE);
	MemoryArena MemoryArenaCreate(MemoryArena& a_memory_source, const size_t a_memory_size);
	void MemoryArenaFree(MemoryArena& a_arena);
	void MemoryArenaReset(MemoryArena& a_arena);

	void TagMemory(const MemoryArena& a_arena, void* a_memory_tag, const char* a_tag_name);

	const MemoryArenaAllocationInfo* MemoryArenaGetFrontAllocationLog(const MemoryArena& a_arena);
	size_t MemoryArenaSizeRemaining(const MemoryArena& a_arena);
	size_t MemoryArenaSizeCommited(const MemoryArena& a_arena);
	size_t MemoryArenaSizeUsed(const MemoryArena& a_arena);

	bool MemoryArenaIsPointerWithinArena(const MemoryArena& a_arena, const void* a_pointer);

	MemoryArenaMarker MemoryArenaGetMemoryMarker(const MemoryArena& a_arena);
	void MemoryArenaSetMemoryMarker(MemoryArena& a_arena, const MemoryArenaMarker& a_memory_marker);

#define MemoryArenaScope(a_memory_arena) \
	for (MemoryArenaMarker _stack_marker = MemoryArenaGetMemoryMarker(a_memory_arena); _stack_marker.at; MemoryArenaSetMemoryMarker(a_memory_arena, _stack_marker), _stack_marker.at = nullptr)

	//don't use this unless you know what you are doing, use ArenaAlloc instead
	void* ArenaAlloc_f(BB_ARENA_DEBUG MemoryArena& a_arena, const size_t a_memory_size, const uint32_t a_align);
	void* ArenaAllocNoZero_f(BB_ARENA_DEBUG MemoryArena& a_arena, const size_t a_memory_size, const uint32_t a_align);
	void* ArenaRealloc_f(BB_ARENA_DEBUG MemoryArena& a_arena, void* a_ptr, const size_t a_ptr_size, const size_t a_memory_size, const uint32_t a_align);
	void* ArenaReallocNoZero_f(BB_ARENA_DEBUG MemoryArena& a_arena, void* a_ptr, const size_t a_ptr_size, const size_t a_memory_size, const uint32_t a_align);

#define ArenaAlloc(a_arena, a_memory_size, a_align) BB::ArenaAlloc_f(BB_ARENA_DEBUG_ARGS a_arena, a_memory_size, a_align)
#define ArenaAllocNoZero(a_arena, a_memory_size, a_align) BB::ArenaAllocNoZero_f(BB_ARENA_DEBUG_ARGS a_arena, a_memory_size, a_align)

#define ArenaAllocType(a_arena, a_type) new (BB::ArenaAlloc_f(BB_ARENA_DEBUG_ARGS a_arena, sizeof(a_type), alignof(a_type))) a_type
#define ArenaAllocTypeNoZero(a_arena, a_type) new (BB::ArenaAllocNoZero_f(BB_ARENA_DEBUG_ARGS a_arena, sizeof(a_type), alignof(a_type))) a_type

#define ArenaAllocArr(a_arena, a_type, a_count) reinterpret_cast<a_type*>(BB::ArenaAlloc_f(BB_ARENA_DEBUG_ARGS a_arena, sizeof(a_type) * a_count, alignof(a_type)))
#define ArenaAllocArrNoZero(a_arena, a_memory_size, a_align) reinterpret_cast<a_type*>(BB::ArenaAllocNoZero_f(BB_ARENA_DEBUG_ARGS a_arena, sizeof(a_type) * a_count, alignof(a_type)))

#define ArenaRealloc(a_arena, a_ptr, a_ptr_size, a_memory_size, a_align) BB::ArenaRealloc_f(BB_ARENA_DEBUG_ARGS a_arena, a_ptr, a_ptr_size, a_memory_size, a_align)
#define ArenaReallocNoZero(a_arena, a_ptr, a_ptr_size, a_memory_size, a_align) BB::ArenaReallocNoZero_f(BB_ARENA_DEBUG_ARGS a_arena, a_ptr, a_ptr_size, a_memory_size, a_align)
}
