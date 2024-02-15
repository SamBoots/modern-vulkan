#include "MemoryInterfaces.hpp"
#include "Logger.h"

using namespace BB;

void FreelistInterface::Initialize(MemoryArena& a_arena, size_t a_memory_size)
{
	m_free_blocks = reinterpret_cast<FreeBlock*>(ArenaAlloc(a_arena, a_memory_size, 16));
	m_memory_size = a_memory_size;
}

void FreelistInterface::Initialize(void* a_memory, size_t a_memory_size)
{
	m_free_blocks = reinterpret_cast<FreeBlock*>(a_memory);
	memset(a_memory, 0, a_memory_size);
	m_memory_size = a_memory_size;
}

void* FreelistInterface::Alloc(size_t a_size, size_t a_alignment)
{
	FreeBlock* previous_free = nullptr;
	FreeBlock* free_block = m_free_blocks;

	while (free_block != nullptr)
	{
		size_t adjustment = Pointer::AlignForwardAdjustmentHeader(free_block, a_alignment, sizeof(AllocHeader));
		size_t total_size = a_size + adjustment;

		if (free_block->size < total_size)
		{
			previous_free = free_block;
			free_block = free_block->next;
			continue;
		}

		if (free_block->size - total_size <= sizeof(AllocHeader))
		{
			total_size = free_block->size;

			if (previous_free != nullptr)
				previous_free->next = free_block->next;
			else
				m_free_blocks = free_block->next;
		}
		else
		{
			FreeBlock* next_block = reinterpret_cast<FreeBlock*>(Pointer::Add(free_block, total_size));

			next_block->size = free_block->size - total_size;
			next_block->next = free_block->next;

			if (previous_free != nullptr)
				previous_free->next = next_block;
			else
				m_free_blocks = next_block;
		}

		uintptr_t address = reinterpret_cast<uintptr_t>(free_block) + adjustment;
		AllocHeader* header = reinterpret_cast<AllocHeader*>(address - sizeof(AllocHeader));
		header->size = total_size;
		header->adjustment = adjustment;

		return reinterpret_cast<void*>(address);
	}
	BB_ASSERT(false, "freelist out of memory");
	return nullptr;
}

void FreelistInterface::Free(void* a_ptr)
{
	BB_ASSERT(a_ptr != nullptr, "Nullptr send to FreelistAllocator::Free!");
	AllocHeader* header = reinterpret_cast<AllocHeader*>(Pointer::Subtract(a_ptr, sizeof(AllocHeader)));
	size_t block_size = header->size;
	uintptr_t block_start = reinterpret_cast<uintptr_t>(a_ptr) - header->adjustment;
	uintptr_t block_end = block_start + block_size;

	FreeBlock* previous_block = nullptr;
	FreeBlock* free_block = m_free_blocks;

	while (free_block != nullptr)
	{
		BB_ASSERT(free_block != free_block->next, "Next points to it's self.");
		uintptr_t t_FreeBlockPos = reinterpret_cast<uintptr_t>(free_block);
		if (t_FreeBlockPos >= block_end) break;
		previous_block = free_block;
		free_block = free_block->next;
	}

	if (previous_block == nullptr)
	{
		previous_block = reinterpret_cast<FreeBlock*>(block_start);
		previous_block->size = header->size;
		previous_block->next = m_free_blocks;
		m_free_blocks = previous_block;
	}
	else if (reinterpret_cast<uintptr_t>(previous_block) + previous_block->size == block_start)
	{
		previous_block->size += block_size;
	}
	else
	{
		FreeBlock* temp = reinterpret_cast<FreeBlock*>(block_start);
		temp->size = block_size;
		temp->next = previous_block->next;
		previous_block->next = temp;
		previous_block = temp;
	}

	if (free_block != nullptr && reinterpret_cast<uintptr_t>(free_block) == block_end)
	{
		previous_block->size += free_block->size;
		previous_block->next = free_block->next;
	}
}

void FreelistInterface::Clear() 
{
	m_free_blocks = reinterpret_cast<FreeBlock*>(m_start);
	m_free_blocks->size = m_memory_size;
	m_free_blocks->next = nullptr;
}