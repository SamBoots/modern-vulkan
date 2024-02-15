#include "MemoryArena.hpp"

namespace BB
{
	struct FreelistInterface
	{
		void Initialize(MemoryArena& a_arena, size_t a_memory_size);
		void Initialize(void* a_memory, size_t a_memory_size);
		void* Alloc(size_t a_size, size_t a_alignment);
		void Free(void* a_ptr);
		void Clear();

		struct AllocHeader
		{
			size_t size;
			size_t adjustment;
		};
		struct FreeBlock
		{
			size_t size;
			FreeBlock* next;
		};

	private:
		uint8_t* m_start = nullptr;
		FreeBlock* m_free_blocks;
		size_t m_memory_size;
	};
}
