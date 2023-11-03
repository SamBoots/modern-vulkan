#include "TemporaryAllocator.h"

namespace BB
{
	void* ReallocTemp(BB_MEMORY_DEBUG_UNUSED void* a_allocator, size_t a_size, size_t a_alignment, void*)
	{
		if (a_size == 0)
			return nullptr;

		return reinterpret_cast<TemporaryAllocator*>(a_allocator)->Alloc(a_size, a_alignment);
	}

	struct TemporaryFreeBlock
	{
		size_t size;
		size_t used;
		TemporaryFreeBlock* previousBlock;
	};

	BB::TemporaryAllocator::operator BB::Allocator()
	{
		Allocator t_AllocatorInterface;
		t_AllocatorInterface.allocator = this;
		t_AllocatorInterface.func = ReallocTemp;
		return t_AllocatorInterface;
	}

	BB::TemporaryAllocator::TemporaryAllocator(Allocator a_BackingAllocator)
	{
		memset(this, 0, sizeof(*this));
		m_BackingAllocator = a_BackingAllocator;
		m_FreeBlock = reinterpret_cast<TemporaryFreeBlock*>(m_Buffer);
		m_FreeBlock->size = sizeof(m_Buffer);
		m_FreeBlock->used = sizeof(TemporaryFreeBlock);
		m_FreeBlock->previousBlock = nullptr;
	}

	BB::TemporaryAllocator::~TemporaryAllocator()
	{
		while (m_FreeBlock->previousBlock != nullptr)
		{
			TemporaryFreeBlock* t_PreviousBlock = m_FreeBlock->previousBlock;
			BBfree(m_BackingAllocator, m_FreeBlock);
			m_FreeBlock = t_PreviousBlock;
		}
	}

	void* BB::TemporaryAllocator::Alloc(size_t a_size, size_t a_alignment)
	{
		size_t t_Adjustment = Pointer::AlignForwardAdjustment(
			Pointer::Add(m_FreeBlock, m_FreeBlock->used),
			a_alignment);

		size_t aligned_size = a_size + t_Adjustment;

		//Does it fit in our current block.
		if (m_FreeBlock->size - m_FreeBlock->used >= aligned_size)
		{
			void* t_Address = Pointer::Add(m_FreeBlock, m_FreeBlock->used);
			m_FreeBlock->used += aligned_size;
			return t_Address;
		}

		//Create new one and double the blocksize
		size_t t_NewBlockSize = m_FreeBlock->used + m_FreeBlock->size;
		//If the allocation is bigger then the new block resize the new block for the allocation.
		//Round up to the new block size for ease of use and correct alignment.
		if (aligned_size > t_NewBlockSize)
			t_NewBlockSize = RoundUp(aligned_size, t_NewBlockSize);

		TemporaryFreeBlock* t_Previous = m_FreeBlock;
		m_FreeBlock = reinterpret_cast<TemporaryFreeBlock*>(BBalloc(m_BackingAllocator, t_NewBlockSize));
		m_FreeBlock->size = t_NewBlockSize;
		m_FreeBlock->used = sizeof(TemporaryFreeBlock);
		m_FreeBlock->previousBlock = t_Previous;

		//Try again with the new block
		return Alloc(a_size, a_alignment);
	}

	void BB::TemporaryAllocator::Clear()
	{
		while (m_FreeBlock->previousBlock != nullptr)
		{
			TemporaryFreeBlock* t_PreviousBlock = m_FreeBlock->previousBlock;
			BBfree(m_BackingAllocator, m_FreeBlock);
			m_FreeBlock = t_PreviousBlock;
		}
		m_FreeBlock->size = sizeof(m_Buffer);
		m_FreeBlock->used = sizeof(TemporaryFreeBlock);
	}
}