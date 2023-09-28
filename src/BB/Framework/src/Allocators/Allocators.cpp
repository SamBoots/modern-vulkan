#include "Allocators.h"
#include "Utils/Utils.h"
#include "Utils/Logger.h"

#include "Storage/BBString.h"

#include "BackingAllocator.h"
#include "OS/Program.h"
#include "BBGlobal.h"

using namespace BB;
using namespace BB::allocators;

#pragma region DEBUG_LOG
#ifdef _DEBUG
constexpr const uintptr_t MEMORY_BOUNDRY_CHECK_VALUE = 0xDEADBEEFDEADBEEF;
enum class BOUNDRY_ERROR
{
	NONE,
	FRONT,
	BACK
};

static BaseAllocator::AllocationLog* DeleteEntry(BaseAllocator::AllocationLog* a_Front, const BaseAllocator::AllocationLog* a_DeletedEntry)
{
	BaseAllocator::AllocationLog* t_Entry = a_Front;
	while (t_Entry->prev != a_DeletedEntry)
	{
		t_Entry = t_Entry->prev;
	}
	t_Entry->prev = a_DeletedEntry->prev;

	return a_Front;
}

//Checks Adds memory boundry to an allocation log.
void* Memory_AddBoundries(void* a_Front, const size_t a_AllocSize)
{
	//Set the begin bound value
	*reinterpret_cast<size_t*>(a_Front) = MEMORY_BOUNDRY_CHECK_VALUE;

	//Set the back bytes.
	void* a_Back = Pointer::Add(a_Front, a_AllocSize - MEMORY_BOUNDRY_BACK);
	*reinterpret_cast<size_t*>(a_Back) = MEMORY_BOUNDRY_CHECK_VALUE;

	return a_Back;
}

//Checks the memory boundries, 
const BOUNDRY_ERROR Memory_CheckBoundries(void* a_Front, void* a_Back)
{
	if (*reinterpret_cast<size_t*>(a_Front) != MEMORY_BOUNDRY_CHECK_VALUE)
		return BOUNDRY_ERROR::FRONT;

	if (*reinterpret_cast<size_t*>(a_Back) != MEMORY_BOUNDRY_CHECK_VALUE)
		return BOUNDRY_ERROR::BACK;

	return BOUNDRY_ERROR::NONE;
}

void* AllocDebug(BB_MEMORY_DEBUG BaseAllocator* a_allocator, const size_t a_size, void* a_AllocatedPtr)
{
	//Get the space for the allocation log, but keep enough space for the boundry check.
	BaseAllocator::AllocationLog* t_AllocLog = reinterpret_cast<BaseAllocator::AllocationLog*>(
		Pointer::Add(a_AllocatedPtr, MEMORY_BOUNDRY_FRONT));

	t_AllocLog->prev = a_allocator->frontLog;
	t_AllocLog->front = a_AllocatedPtr;
	t_AllocLog->back = Memory_AddBoundries(a_AllocatedPtr, a_size);
	t_AllocLog->allocSize = static_cast<uint32_t>(a_size);
	t_AllocLog->file = a_File;
	t_AllocLog->line = a_Line;
	t_AllocLog->tagName = "";
	//set the new front log.
	a_allocator->frontLog = t_AllocLog;
	return Pointer::Add(a_AllocatedPtr, MEMORY_BOUNDRY_FRONT + sizeof(BaseAllocator::AllocationLog));
}

void* FreeDebug(BaseAllocator* a_allocator, void* a_ptr)
{
	const BaseAllocator::AllocationLog* t_AllocLog = reinterpret_cast<BaseAllocator::AllocationLog*>(
		Pointer::Subtract(a_ptr, sizeof(BaseAllocator::AllocationLog)));

	const BOUNDRY_ERROR t_HasError = Memory_CheckBoundries(t_AllocLog->front, t_AllocLog->back);
	switch (t_HasError)
	{
	case BOUNDRY_ERROR::FRONT:
		//We call it explictally since we can avoid the macro and pull in the file + name directly to Log_Error. 
		Logger::Log_Assert(t_AllocLog->file, t_AllocLog->line, "ss",
			t_AllocLog->tagName,
			"Memory Boundry overwritten at the front of memory block.");
		break;
	case BOUNDRY_ERROR::BACK:
		//We call it explictally since we can avoid the macro and pull in the file + name directly to Log_Error. 
		Logger::Log_Assert(t_AllocLog->file, t_AllocLog->line, "ss",
			t_AllocLog->tagName,
			"Memory Boundry overwritten at the back of memory block.");
		break;
	}

	a_ptr = Pointer::Subtract(a_ptr, MEMORY_BOUNDRY_FRONT + sizeof(BaseAllocator::AllocationLog));

	BaseAllocator::AllocationLog* t_FrontLog = a_allocator->frontLog;

	if (t_AllocLog != a_allocator->frontLog)
		DeleteEntry(t_FrontLog, t_AllocLog);
	else
		a_allocator->frontLog = t_FrontLog->prev;

	return a_ptr;
}
#endif //_DEBUG
#pragma endregion DEBUG

void BB::allocators::BaseAllocator::Validate() const
{
#ifdef _DEBUG
	AllocationLog* t_FrontLog = frontLog;
	while (t_FrontLog != nullptr)
	{
		Memory_CheckBoundries(frontLog->front, frontLog->back);

		BB::StackString<256> t_TempString;
		{
			char t_AllocBegin[]{ "Memory leak accured! Check file and line number for leak location \nAllocator name:" };
			t_TempString.append(t_AllocBegin, sizeof(t_AllocBegin) - 1);
			t_TempString.append(name);
		}
		{
			char t_Begin[]{ "\nLeak size:" };
			t_TempString.append(t_Begin, sizeof(t_Begin) - 1);
			
			char t_LeakSize[16]{};
			sprintf_s(t_LeakSize, 15, "%u", static_cast<uint32_t>(t_FrontLog->allocSize));
			t_TempString.append(t_LeakSize);
		}
	

		Logger::Log_Assert(t_FrontLog->file, t_FrontLog->line, "ss",
			t_FrontLog->tagName, t_TempString.c_str());

		t_FrontLog = t_FrontLog->prev;
	}
#endif //_DEBUG
}

void BB::allocators::BaseAllocator::Clear()
{
#ifdef _DEBUG
	while (frontLog != nullptr)
	{
		Memory_CheckBoundries(frontLog->front, frontLog->back);
		frontLog = frontLog->prev;
	}
#endif //_DEBUG
}

void* LinearRealloc(BB_MEMORY_DEBUG void* a_allocator, size_t a_size, const size_t a_Alignment, void* a_ptr)
{
	LinearAllocator* t_Linear = reinterpret_cast<LinearAllocator*>(a_allocator);
	BB_ASSERT(a_ptr == nullptr, "Trying to free a pointer on a linear allocator!");
#ifdef _DEBUG
	a_size += MEMORY_BOUNDRY_FRONT + MEMORY_BOUNDRY_BACK + sizeof(BaseAllocator::AllocationLog);
#endif //_DEBUG
	void* t_AllocatedPtr = t_Linear->Alloc(a_size, a_Alignment);
#ifdef _DEBUG
	t_AllocatedPtr = AllocDebug(a_File, a_Line, t_Linear, a_size, t_AllocatedPtr);
#endif //_DEBUG
	return t_AllocatedPtr;
};

LinearAllocator::LinearAllocator(const size_t a_size, const char* a_name)
	: BaseAllocator(a_name)
{
	BB_ASSERT(a_size != 0, "linear allocator is created with a size of 0!");
	size_t size = a_size;
	m_start = mallocVirtual(nullptr, size);
	m_buffer = m_start;
	m_end = reinterpret_cast<uintptr_t>(m_start) + a_size;
}

LinearAllocator::~LinearAllocator()
{
	Validate();
	freeVirtual(reinterpret_cast<void*>(m_start));
}

LinearAllocator::operator Allocator()
{
	Allocator t_AllocatorInterface;
	t_AllocatorInterface.allocator = this;
	t_AllocatorInterface.func = LinearRealloc;
	return t_AllocatorInterface;
}

void* LinearAllocator::Alloc(size_t a_size, size_t a_alignment)
{
	size_t t_Adjustment = Pointer::AlignForwardAdjustment(m_buffer, a_alignment);

	uintptr_t t_Address = reinterpret_cast<uintptr_t>(Pointer::Add(m_buffer, t_Adjustment));
	m_buffer = reinterpret_cast<void*>(t_Address + a_size);

	if (t_Address + a_size > m_end)
	{
		size_t increase = Max(a_size, (m_end - reinterpret_cast<uintptr_t>(m_start)));
		mallocVirtual(m_start, increase);
		m_end += increase;
	}

	return reinterpret_cast<void*>(t_Address);
}

void LinearAllocator::Free(void*)
{
	BB_WARNING(false, "Tried to free a piece of memory in a linear allocator, warning will be removed when temporary allocators exist!", WarningType::LOW);
}

void LinearAllocator::Clear()
{
	BaseAllocator::Clear();
	m_buffer = m_start;
}

FixedLinearAllocator::FixedLinearAllocator(const size_t a_size, const char* a_Name)
	: BaseAllocator(a_Name)
{
	BB_ASSERT(a_size != 0, "Fixed linear allocator is created with a size of 0!");
	size_t t_Size = a_size;
	m_start = mallocVirtual(nullptr, t_Size, VIRTUAL_RESERVE_NONE);
	m_Buffer = m_start;
#ifdef _DEBUG
	m_End = reinterpret_cast<uintptr_t>(m_start) + t_Size;
#endif //_DEBUG
}

FixedLinearAllocator::~FixedLinearAllocator()
{
	Validate();
	freeVirtual(reinterpret_cast<void*>(m_start));
}

FixedLinearAllocator::operator Allocator()
{
	Allocator t_AllocatorInterface;
	t_AllocatorInterface.allocator = this;
	t_AllocatorInterface.func = LinearRealloc;
	return t_AllocatorInterface;
}

void* FixedLinearAllocator::Alloc(size_t a_size, size_t a_Alignment)
{
	size_t t_Adjustment = Pointer::AlignForwardAdjustment(m_Buffer, a_Alignment);

	uintptr_t t_Address = reinterpret_cast<uintptr_t>(Pointer::Add(m_Buffer, t_Adjustment));
	m_Buffer = reinterpret_cast<void*>(t_Address + a_size);

#ifdef _DEBUG
	if (t_Address + a_size > m_End)
	{
		BB_ASSERT(false, "Failed to allocate more memory from a fixed linear allocator");
	}
#endif //_DEBUG
	return reinterpret_cast<void*>(t_Address);
}

void FixedLinearAllocator::Free(void*)
{
	BB_WARNING(false, "Tried to free a piece of memory in a linear allocator, warning will be removed when temporary allocators exist!", WarningType::LOW);
}

void FixedLinearAllocator::Clear()
{
	BaseAllocator::Clear();
	m_Buffer = m_start;
}

StackAllocator::StackAllocator(const size_t a_size, const char* a_name)
	: BaseAllocator(a_name)
{
	BB_ASSERT(a_size != 0, "linear allocator is created with a size of 0!");
	size_t size = a_size;
	m_start = mallocVirtual(nullptr, size);
	m_buffer = m_start;
	m_end = reinterpret_cast<uintptr_t>(m_start) + size;
}

StackAllocator::~StackAllocator()
{
	Validate();
	freeVirtual(reinterpret_cast<void*>(m_start));
}

StackAllocator::operator Allocator()
{
	Allocator t_AllocatorInterface;
	t_AllocatorInterface.allocator = this;
	t_AllocatorInterface.func = LinearRealloc;
	return t_AllocatorInterface;
}

void* StackAllocator::Alloc(size_t a_size, size_t a_alignment)
{
	size_t adjustment = Pointer::AlignForwardAdjustment(m_buffer, a_alignment);

	uintptr_t address = reinterpret_cast<uintptr_t>(Pointer::Add(m_buffer, adjustment));
	m_buffer = reinterpret_cast<void*>(address + a_size);

	if (address + a_size > m_end)
	{
		size_t increase = Max(a_size, (m_end - reinterpret_cast<uintptr_t>(m_start)));
		mallocVirtual(m_start, increase);
		m_end += increase;
	}

	return reinterpret_cast<void*>(address);
}

void StackAllocator::Free(void*)
{
	BB_WARNING(false, "Tried to free a piece of memory in a linear allocator, warning will be removed when temporary allocators exist!", WarningType::LOW);
}

void StackAllocator::Clear()
{
	BaseAllocator::Clear();
	m_buffer = m_start;
}

void StackAllocator::SetMarker(const StackMarker a_marker)
{
	BB_ASSERT(reinterpret_cast<uintptr_t>(m_start) <= a_marker && a_marker < m_end, "stack position is not within this allocator's memory space");
#ifdef _DEBUG
	if (a_marker == reinterpret_cast<uintptr_t>(m_buffer))
		return;
	//jank, but remove logs that are after a_pos;
	AllocationLog* cur_list = frontLog;
	AllocationLog* prev_list = nullptr;
	uintptr_t front = reinterpret_cast<uintptr_t>(cur_list->front);
	while (front > a_marker)
	{
		prev_list = cur_list;
		cur_list = prev_list->prev;
		front = reinterpret_cast<uintptr_t>(cur_list->front);
	}
	BB_ASSERT(a_marker == front, "SetPosition points to a invalid address that holds no allocation");
	frontLog = cur_list->prev;
#endif
	m_buffer = reinterpret_cast<void*>(a_marker);
}

void* FreelistRealloc(BB_MEMORY_DEBUG void* a_allocator, size_t a_size, const size_t a_Alignment, void* a_ptr)
{
	FreelistAllocator* t_Freelist = reinterpret_cast<FreelistAllocator*>(a_allocator);
	if (a_size > 0)
	{
#ifdef _DEBUG
		a_size += MEMORY_BOUNDRY_FRONT + MEMORY_BOUNDRY_BACK + sizeof(BaseAllocator::AllocationLog);
#endif //_DEBUG
		void* t_AllocatedPtr = t_Freelist->Alloc(a_size, a_Alignment);
#ifdef _DEBUG
		t_AllocatedPtr = AllocDebug(a_File, a_Line, t_Freelist, a_size, t_AllocatedPtr);
#endif //_DEBUG
		return t_AllocatedPtr;
	}
	else
	{
#ifdef _DEBUG
		a_ptr = FreeDebug(t_Freelist, a_ptr);
#endif //_DEBUG
		t_Freelist->Free(a_ptr);
		return nullptr;
	}
};

FreelistAllocator::FreelistAllocator(const size_t a_size, const char* a_Name)
	: BaseAllocator(a_Name)
{
	BB_ASSERT(a_size != 0, "Freelist allocator is created with a size of 0!");
	BB_WARNING(a_size > 10240, "Freelist allocator is smaller then 10 kb, you generally want a bigger freelist.", WarningType::OPTIMALIZATION);
	m_TotalAllocSize = a_size;
	m_start = reinterpret_cast<uint8_t*>(mallocVirtual(nullptr, m_TotalAllocSize));
	m_FreeBlocks = reinterpret_cast<FreeBlock*>(m_start);
	m_FreeBlocks->size = m_TotalAllocSize;
	m_FreeBlocks->next = nullptr;
}

FreelistAllocator::~FreelistAllocator()
{
	Validate();
	freeVirtual(m_start);
}

FreelistAllocator::operator Allocator()
{
	Allocator t_AllocatorInterface;
	t_AllocatorInterface.allocator = this;
	t_AllocatorInterface.func = FreelistRealloc;
	return t_AllocatorInterface;
}

void* FreelistAllocator::Alloc(size_t a_size, size_t a_Alignment)
{
	FreeBlock* t_PreviousFreeBlock = nullptr;
	FreeBlock* t_FreeBlock = m_FreeBlocks;

	while (t_FreeBlock != nullptr)
	{
		size_t t_Adjustment = Pointer::AlignForwardAdjustmentHeader(t_FreeBlock, a_Alignment, sizeof(AllocHeader));
		size_t t_TotalSize = a_size + t_Adjustment;

		if (t_FreeBlock->size < t_TotalSize)
		{
			t_PreviousFreeBlock = t_FreeBlock;
			t_FreeBlock = t_FreeBlock->next;
			continue;
		}

		if (t_FreeBlock->size - t_TotalSize <= sizeof(AllocHeader))
		{
			t_TotalSize = t_FreeBlock->size;

			if (t_PreviousFreeBlock != nullptr)
				t_PreviousFreeBlock->next = t_FreeBlock->next;
			else
				m_FreeBlocks = t_FreeBlock->next;
		}
		else
		{
			FreeBlock* t_NextBlock = reinterpret_cast<FreeBlock*>(Pointer::Add(t_FreeBlock, t_TotalSize));

			t_NextBlock->size = t_FreeBlock->size - t_TotalSize;
			t_NextBlock->next = t_FreeBlock->next;

			if (t_PreviousFreeBlock != nullptr)
				t_PreviousFreeBlock->next = t_NextBlock;
			else
				m_FreeBlocks = t_NextBlock;
		}

		uintptr_t t_Address = reinterpret_cast<uintptr_t>(t_FreeBlock) + t_Adjustment;
		AllocHeader* t_Header = reinterpret_cast<AllocHeader*>(t_Address - sizeof(AllocHeader));
		t_Header->size = t_TotalSize;
		t_Header->adjustment = t_Adjustment;

		return reinterpret_cast<void*>(t_Address);
	}
	BB_WARNING(false, "Increasing the size of a freelist allocator, risk of fragmented memory.", WarningType::OPTIMALIZATION);
	//Double the size of the freelist.
	FreeBlock* t_NewAllocBlock = reinterpret_cast<FreeBlock*>(mallocVirtual(m_start, m_TotalAllocSize));
	t_NewAllocBlock->size = m_TotalAllocSize;
	t_NewAllocBlock->next = m_FreeBlocks;

	//Update the new total alloc size.
	m_TotalAllocSize += m_TotalAllocSize;

	//Set the new block as the main block.
	m_FreeBlocks = t_NewAllocBlock;

	return this->Alloc(a_size, a_Alignment);
}

void FreelistAllocator::Free(void* a_ptr)
{
	BB_ASSERT(a_ptr != nullptr, "Nullptr send to FreelistAllocator::Free!.");
	AllocHeader* t_Header = reinterpret_cast<AllocHeader*>(Pointer::Subtract(a_ptr, sizeof(AllocHeader)));
	size_t t_BlockSize = t_Header->size;
	uintptr_t t_BlockStart = reinterpret_cast<uintptr_t>(a_ptr) - t_Header->adjustment;
	uintptr_t t_BlockEnd = t_BlockStart + t_BlockSize;

	FreeBlock* t_PreviousBlock = nullptr;
	FreeBlock* t_FreeBlock = m_FreeBlocks;

	while (t_FreeBlock != nullptr)
	{
		BB_ASSERT(t_FreeBlock != t_FreeBlock->next, "Next points to it's self.");
		uintptr_t t_FreeBlockPos = reinterpret_cast<uintptr_t>(t_FreeBlock);
		if (t_FreeBlockPos >= t_BlockEnd) break;
		t_PreviousBlock = t_FreeBlock;
		t_FreeBlock = t_FreeBlock->next;
	}

	if (t_PreviousBlock == nullptr)
	{
		t_PreviousBlock = reinterpret_cast<FreeBlock*>(t_BlockStart);
		t_PreviousBlock->size = t_Header->size;
		t_PreviousBlock->next = m_FreeBlocks;
		m_FreeBlocks = t_PreviousBlock;
	}
	else if (reinterpret_cast<uintptr_t>(t_PreviousBlock) + t_PreviousBlock->size == t_BlockStart)
	{
		t_PreviousBlock->size += t_BlockSize;
	}
	else
	{
		FreeBlock* t_Temp = reinterpret_cast<FreeBlock*>(t_BlockStart);
		t_Temp->size = t_BlockSize;
		t_Temp->next = t_PreviousBlock->next;
		t_PreviousBlock->next = t_Temp;
		t_PreviousBlock = t_Temp;
	}

	if (t_FreeBlock != nullptr && reinterpret_cast<uintptr_t>(t_FreeBlock) == t_BlockEnd)
	{
		t_PreviousBlock->size += t_FreeBlock->size;
		t_PreviousBlock->next = t_FreeBlock->next;
	}
}

void BB::allocators::FreelistAllocator::Clear()
{
	BaseAllocator::Clear();
	m_FreeBlocks = reinterpret_cast<FreeBlock*>(m_start);
	m_FreeBlocks->size = m_TotalAllocSize;
	m_FreeBlocks->next = nullptr;
}

BB::allocators::POW_FreelistAllocator::POW_FreelistAllocator(const size_t, const char* a_Name)
	: BaseAllocator(a_Name)
{
	constexpr const size_t MIN_FREELIST_SIZE = 32;
	constexpr const size_t FREELIST_START_SIZE = 12;

	size_t t_Freelist_Buffer_Size = MIN_FREELIST_SIZE;
	m_FreeBlocksAmount = FREELIST_START_SIZE;

	//This will be resized accordingly by mallocVirtual.
	size_t t_FreeListAllocSize = sizeof(FreeList) * 12;

	//Get memory to store the headers for all the freelists.
	//reserve none extra since this will never be bigger then the virtual alloc maximum. (If it is then we should get a page fault).
	m_FreeLists = reinterpret_cast<FreeList*>(mallocVirtual(nullptr, t_FreeListAllocSize, VIRTUAL_RESERVE_HALF));

	//Set the freelists and let the blocks point to the next free ones.
	for (size_t i = 0; i < m_FreeBlocksAmount; i++)
	{
		//Roundup the freelist with the virtual memory page size for the most optimal allocation. 
		size_t t_UsedMemory = RoundUp(OSPageSize(), t_Freelist_Buffer_Size);
		m_FreeLists[i].allocSize = t_Freelist_Buffer_Size;
		m_FreeLists[i].fullSize = t_UsedMemory;
		//reserve half since we are splitting up the block, otherwise we might use a lot of virtual space.
		m_FreeLists[i].start = mallocVirtual(nullptr, t_UsedMemory, VIRTUAL_RESERVE_HALF);
		m_FreeLists[i].freeBlock = reinterpret_cast<FreeBlock*>(m_FreeLists[i].start);
		//Set the first freeblock.
		m_FreeLists[i].freeBlock->size = m_FreeLists[i].fullSize;
		m_FreeLists[i].freeBlock->next = nullptr;
		t_Freelist_Buffer_Size *= 2;
	}
}

BB::allocators::POW_FreelistAllocator::~POW_FreelistAllocator()
{
	Validate();
	for (size_t i = 0; i < m_FreeBlocksAmount; i++)
	{
		//Free all the free lists
		freeVirtual(m_FreeLists[i].start);
	}

	//Free the freelist holder.
	freeVirtual(m_FreeLists);
}

POW_FreelistAllocator::operator Allocator()
{
	Allocator t_AllocatorInterface;
	t_AllocatorInterface.allocator = this;
	t_AllocatorInterface.func = FreelistRealloc;
	return t_AllocatorInterface;
}

void* BB::allocators::POW_FreelistAllocator::Alloc(size_t a_size, size_t)
{
	FreeList* t_FreeList = m_FreeLists;
	const size_t t_TotalAlloc = a_size + sizeof(AllocHeader);
	//Get the right freelist for the allocation
	while (t_TotalAlloc >= t_FreeList->allocSize)
	{
		t_FreeList++;
	}

	if (t_FreeList->freeBlock != nullptr)
	{
		FreeBlock* t_FreeBlock = t_FreeList->freeBlock;

		FreeBlock* t_NewBlock = reinterpret_cast<FreeBlock*>(Pointer::Add(t_FreeList->freeBlock, t_FreeList->allocSize));
		t_NewBlock->size = t_FreeBlock->size - t_FreeList->allocSize;
		t_NewBlock->next = t_FreeBlock->next;

		//If we cannot support enough memory for the next allocation, allocate more memory.
		//The reasoning behind it is that it commits more memory in virtual alloc, which won't commit it to RAM yet.
		//So there is no cost yet, until we write to it.
		if (t_FreeBlock->size < t_FreeList->allocSize)
		{
			if (t_FreeBlock->next != nullptr && t_FreeList->freeBlock->size == 0)
			{
				t_FreeBlock = t_FreeBlock->next;
			}
			else
			{
				//double the size of the freelist, since the block that triggers this condition is always the end we will extend the current block.
				mallocVirtual(t_FreeList->start, t_FreeList->fullSize);
				t_FreeBlock->size += t_FreeList->fullSize;
				t_FreeList->fullSize += t_FreeList->fullSize;
			}
		}

		t_FreeList->freeBlock = t_NewBlock;

		//Place the freelist into the allocation so that it can go back to this.
		reinterpret_cast<AllocHeader*>(t_FreeBlock)->freeList = t_FreeList;

		return Pointer::Add(t_FreeBlock, sizeof(AllocHeader));
	}

	BB_ASSERT(false, "POW_FreelistAllocator either has not enough memory or it doesn't support a size of this allocation.");
	return nullptr;
}

void BB::allocators::POW_FreelistAllocator::Free(void* a_ptr)
{
	AllocHeader* t_Address = static_cast<AllocHeader*>(Pointer::Subtract(a_ptr, sizeof(AllocHeader)));
	FreeList* t_FreeList = t_Address->freeList;

	FreeBlock* t_NewFreeBlock = reinterpret_cast<FreeBlock*>(t_Address);
	t_NewFreeBlock->size = t_Address->freeList->allocSize;
	t_NewFreeBlock->next = t_FreeList->freeBlock;

	t_FreeList->freeBlock = t_NewFreeBlock;
}

void BB::allocators::POW_FreelistAllocator::Clear()
{
	BaseAllocator::Clear();
	//Clear all freeblocks again
	for (size_t i = 0; i < m_FreeBlocksAmount; i++)
	{
		//Reset the freeblock to the start
		m_FreeLists[i].freeBlock = reinterpret_cast<FreeBlock*>(m_FreeLists[i].start);
		m_FreeLists[i].freeBlock->size = m_FreeLists[i].fullSize;
		m_FreeLists[i].freeBlock->next = nullptr;
	}
}

//BB::allocators::PoolAllocator::PoolAllocator(const size_t a_objectSize, const size_t a_objectCount, const size_t a_Alignment)
//{
//	BB_ASSERT(a_objectSize != 0, "Pool allocator is created with an object size of 0!");
//	BB_ASSERT(a_objectCount != 0, "Pool allocator is created with an object count of 0!");
//	//BB_WARNING(a_objectSize * a_objectCount > 10240, "Pool allocator is smaller then 10 kb, might be too small.");
//
//	size_t t_PoolAllocSize = a_objectSize * a_objectCount;
//	m_ObjectCount = a_objectCount;
//	m_start = reinterpret_cast<void**>(mallocVirtual(m_start, t_PoolAllocSize));
//	m_Alignment = pointerutils::alignForwardAdjustment(m_start, a_Alignment);
//	m_start = reinterpret_cast<void**>(pointerutils::Add(m_start, m_Alignment));
//	m_Pool = m_start;
//
//	void** t_Pool = m_Pool;
//
//	for (size_t i = 0; i < m_ObjectCount - 1; i++)
//	{
//		*t_Pool = pointerutils::Add(t_Pool, a_objectSize);
//		t_Pool = reinterpret_cast<void**>(*t_Pool);
//	}
//	*t_Pool = nullptr;
//}
//
//BB::allocators::PoolAllocator::~PoolAllocator()
//{
//	freeVirtual(m_start);
//}
//
//void* BB::allocators::PoolAllocator::Alloc(size_t a_size, size_t)
//{
//	void* t_Item = m_Pool;
//
//	//Increase the Pool allocator by double
//	if (t_Item == nullptr)
//	{
//		size_t t_Increase = m_ObjectCount;
//		size_t t_ByteIncrease = m_ObjectCount * a_size;
//		mallocVirtual(m_start, t_ByteIncrease);
//		void** t_Pool = reinterpret_cast<void**>(pointerutils::Add(m_start, m_ObjectCount * a_size + m_Alignment));
//		m_Pool = t_Pool;
//		m_ObjectCount += m_ObjectCount;
//		
//		for (size_t i = 0; i < t_Increase - 1; i++)
//		{
//			*t_Pool = pointerutils::Add(t_Pool, a_size);
//			t_Pool = reinterpret_cast<void**>(*t_Pool);
//		}
//
//		t_Pool = nullptr;
//		return Alloc(a_size, 0);
//	}
//	m_Pool = reinterpret_cast<void**>(*m_Pool);
//	return t_Item;
//}
//
//void BB::allocators::PoolAllocator::Free(void* a_ptr)
//{
//	(*reinterpret_cast<void**>(a_ptr)) = m_Pool;
//	m_Pool = reinterpret_cast<void**>(a_ptr);
//}
//
//void BB::allocators::PoolAllocator::Clear()
//{
//	m_Pool = reinterpret_cast<void**>(m_start);
//}
