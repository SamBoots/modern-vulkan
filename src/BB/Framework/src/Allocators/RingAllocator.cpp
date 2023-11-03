#include "RingAllocator.h"
#include "BackingAllocator.h"

using namespace BB;

void* ReallocRing(BB_MEMORY_DEBUG void* a_allocator, size_t a_size, size_t a_alignment, void*)
{
	if (a_size == 0)
		return nullptr;

	return reinterpret_cast<RingAllocator*>(a_allocator)->Alloc(a_size, a_alignment);
}

RingAllocator::operator BB::Allocator()
{
	Allocator t_AllocatorInterface;
	t_AllocatorInterface.allocator = this;
	t_AllocatorInterface.func = ReallocRing;
	return t_AllocatorInterface;
}

RingAllocator::RingAllocator(const Allocator a_BackingAllocator, const size_t a_size)
	:	m_size(static_cast<uint32_t>(a_size)), m_BackingAllocator(a_BackingAllocator)
{
	BB_ASSERT(m_size < UINT32_MAX, 
		"Ring allocator's size is larger then UINT32_MAX. This will not work as the counters inside are 32 bit intergers.!");
	
	m_BufferPos = BBalloc(m_BackingAllocator, a_size);
	m_Used = 0;
}

RingAllocator::~RingAllocator()
{
	m_BufferPos = Pointer::Subtract(m_BufferPos, m_Used);
	BBfree(m_BackingAllocator, m_BufferPos);
}

void* RingAllocator::Alloc(size_t a_size, size_t a_alignment)
{
	size_t t_Adjustment = Pointer::AlignForwardAdjustment(m_BufferPos, a_alignment);
	size_t t_AdjustedSize = a_size + a_alignment;
	//Go back to the buffer start if we cannot fill this allocation.
	if (m_Used + t_AdjustedSize > m_size)
	{
		BB_ASSERT(m_size > t_AdjustedSize,
			"Ring allocator tries to allocate something bigger then it's allocator size!");

		m_BufferPos = Pointer::Subtract(m_BufferPos, m_Used);
		m_Used = 0;
		return Alloc(a_size, a_alignment);
	}

	void* t_ReturnPtr = Pointer::Add(m_BufferPos, t_Adjustment);
	m_BufferPos = Pointer::Add(m_BufferPos, t_AdjustedSize);
	m_Used += static_cast<uint32_t>(t_AdjustedSize);

	return t_ReturnPtr;
}




void* ReallocLocalRing(BB_MEMORY_DEBUG_UNUSED void* a_allocator, size_t a_size, size_t a_alignment, void*)
{
	if (a_size == 0)
		return nullptr;

	return reinterpret_cast<LocalRingAllocator*>(a_allocator)->Alloc(a_size, a_alignment);
}

LocalRingAllocator::operator BB::Allocator()
{
	Allocator t_AllocatorInterface;
	t_AllocatorInterface.allocator = this;
	t_AllocatorInterface.func = ReallocLocalRing;
	return t_AllocatorInterface;
}

LocalRingAllocator::LocalRingAllocator(size_t& a_size)
{
	m_BufferPos = mallocVirtual(nullptr, a_size, VIRTUAL_RESERVE_NONE);

	m_size = a_size;
	m_Used = 0;
}

LocalRingAllocator::~LocalRingAllocator()
{
	m_BufferPos = Pointer::Subtract(m_BufferPos, m_Used);
	freeVirtual(m_BufferPos);
}

void* LocalRingAllocator::Alloc(size_t a_size, size_t a_alignment)
{
	size_t t_Adjustment = Pointer::AlignForwardAdjustment(m_BufferPos, a_alignment);
	size_t t_AdjustedSize = a_size + a_alignment;
	//Go back to the buffer start if we cannot fill this allocation.
	if (m_Used + t_AdjustedSize > m_size)
	{
		BB_ASSERT(m_size > t_AdjustedSize,
			"Ring allocator tries to allocate something bigger then it's allocator size!");

		m_BufferPos = Pointer::Subtract(m_BufferPos, m_Used);
		m_Used = 0;
		return Alloc(a_size, a_alignment);
	}

	void* t_ReturnPtr = Pointer::Add(m_BufferPos, t_Adjustment);
	m_BufferPos = Pointer::Add(m_BufferPos, t_AdjustedSize);
	m_Used += t_AdjustedSize;

	return t_ReturnPtr;
}