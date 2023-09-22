#include "Utils/Logger.h"
#include "BackingAllocator.h"
#include "Utils/Utils.h"
#include "OS/Program.h"
#include "Math.inl"
#include "BBGlobal.h"

using namespace BB;

#ifdef _64BIT
constexpr size_t VIRTUAL_HEADER_TYPE_CHECK = 0xB0AFB0AFB0AFB0AF;
#elif _32BIT
constexpr size_t VIRTUAL_HEADER_TYPE_CHECK = 0xB0AFB0AF;
#endif _32BIT

struct VirtualHeader
{
#if _DEBUG
	size_t check_value;
#endif //_DEBUG
	size_t bytes_commited;
	size_t bytes_reserved;
};

void* BB::mallocVirtual(void* a_start, size_t& a_size, const size_t a_reserve_size)
{
	//Adjust the requested bytes by the page size and the minimum virtual allocaion size.
	const size_t page_adjusted_size = RoundUp(a_size + sizeof(VirtualHeader), OSPageSize());

	//Set the reference of a_size so that the allocator has enough memory until the end of the page.
	a_size = page_adjusted_size - sizeof(VirtualHeader);

	//Check the pageHeader
	if (a_start != nullptr)
	{
		//Get the header for preperation to resize it.
		VirtualHeader* page_header = reinterpret_cast<VirtualHeader*>(Pointer::Subtract(a_start, sizeof(VirtualHeader)));
#if _DEBUG
		BB_ASSERT(page_header->check_value == VIRTUAL_HEADER_TYPE_CHECK, "Send a pointer that is NOT a start of a virtual allocation!");
#endif //_DEBUG
		//Commit more memory if there is enough reserved.
		if (page_header->bytes_reserved > page_adjusted_size + page_header->bytes_commited)
		{
			void* new_commit_address = Pointer::Add(page_header, page_header->bytes_commited);

			page_header->bytes_commited += page_adjusted_size;
			BB_ASSERT(CommitVirtualMemory(page_header, page_header->bytes_commited) != 0, "Error commiting virtual memory");
			return new_commit_address;
		}

		BB_ASSERT(false, "Going over reserved memory! Make sure to reserve more memory");
	}

	//When making a new header reserve a lot more then that is requested to support later resizes better.
	const size_t additional_reserve = page_adjusted_size * a_reserve_size;
	void* address = ReserveVirtualMemory(additional_reserve);
	BB_ASSERT(address != NULL, "Error reserving virtual memory");

	//Now commit enough memory that the user requested.
	BB_ASSERT(CommitVirtualMemory(address, page_adjusted_size) != NULL, "Error commiting right after a reserve virtual memory");

	//Set the header of the allocator, used for later resizes and when you need to free it.
#if _DEBUG
	reinterpret_cast<VirtualHeader*>(address)->check_value = VIRTUAL_HEADER_TYPE_CHECK;
#endif //_DEBUG
	reinterpret_cast<VirtualHeader*>(address)->bytes_commited = page_adjusted_size;
	reinterpret_cast<VirtualHeader*>(address)->bytes_reserved = additional_reserve;

	//Return the pointer that does not include the StartPageHeader
	return Pointer::Add(address, sizeof(VirtualHeader));
}

void BB::freeVirtual(void* a_ptr)
{
	BB_ASSERT(ReleaseVirtualMemory(Pointer::Subtract(a_ptr, sizeof(VirtualHeader))) != 0, "Error on releasing virtual memory");
}

//#pragma region Unit Test
//
//#pragma warning (push, 0)
//#include <gtest/gtest.h>
//#pragma warning (pop)
//
//struct MockAllocator
//{
//	MockAllocator(size_t a_size)
//	{
//		start = reinterpret_cast<uint8_t*>(mallocVirtual(start, a_size));
//		maxSize = a_size;
//		//using memset because the memory is NOT commited to ram unless it's accessed.
//		memset(start, 5215, a_size);
//		buffer = start;
//	}
//
//	~MockAllocator()
//	{
//		freeVirtual(start);
//	}
//
//	void* Alloc(size_t a_size)
//	{
//		void* t_Address = buffer;
//		currentSize += a_size;
//
//		if (currentSize > maxSize)
//		{
//			size_t t_BufferIncrease{};
//			if (maxSize > a_size)
//				t_BufferIncrease = Math::RoundUp(a_size, maxSize);
//			else
//				t_BufferIncrease = a_size;
//
//			mallocVirtual(start, t_BufferIncrease);
//			//using memset because the memory is NOT commited to ram unless it's accessed.
//			maxSize += t_BufferIncrease;
//		}
//		memset(buffer, 16, a_size);
//		buffer = Pointer::Add(buffer, a_size);
//		return t_Address;
//	};
//
//	const size_t SpaceLeft() const
//	{
//		return maxSize - currentSize;
//	}
//	//Not supporting free yet.
//	//void free(size_t a_size);
//
//	size_t maxSize;
//	size_t currentSize = 0;
//	void* start = nullptr;
//	void* buffer;
//};
//
//TEST(MemoryAllocators_Backend_Windows, COMMIT_RESERVE_PAGES)
//{
//	//Allocator size is equal to half a page, it will allocate an entire page in the background anyway.
//	MockAllocator t_Allocator(AppOSDevice().VirtualMemoryMinimumAllocation());
//	ASSERT_EQ(AppOSDevice().LatestOSError(), 0x0) << "Windows API error on creating the MockAllocator.";
//
//	VirtualHeader lastHeader = *reinterpret_cast<VirtualHeader*>(Pointer::Subtract(t_Allocator.start, sizeof(VirtualHeader)));
//	VirtualHeader newHeader;
//
//	//Allocate memory equal to an entire page, this should increase the commited amount of pages, but not reserved.
//	t_Allocator.Alloc(AppOSDevice().VirtualMemoryMinimumAllocation() * 3);
//	ASSERT_EQ(AppOSDevice().LatestOSError(), 0x0) << "Windows API error on commiting more memory.";
//	newHeader = *reinterpret_cast<VirtualHeader*>(Pointer::Subtract(t_Allocator.start, sizeof(VirtualHeader)));
//	EXPECT_NE(lastHeader.bytesCommited, newHeader.bytesCommited) << "Bytes commited is not changed, while it should change!";
//	//Reserved is now never changed.
//	//EXPECT_NE(lastHeader.bytes_reserved, newHeader.bytes_reserved) << "Bytes reserved is not changed, while it should change!";
//	lastHeader = newHeader;
//}
//#pragma endregion