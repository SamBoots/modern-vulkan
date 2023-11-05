#pragma once
#include "Common.h"

namespace BB
{	
	constexpr const size_t MEMORY_BOUNDRY_FRONT = sizeof(size_t);
	constexpr const size_t MEMORY_BOUNDRY_BACK = sizeof(size_t);

	namespace allocators
	{
		struct BaseAllocator
		{
			BaseAllocator(const char* a_Name = "unnamed") { name = a_Name; }

			//realloc is the single allocation call that we make.
			void ClearDebugList();

			//just delete these for safety, copies might cause errors.
			BaseAllocator(const BaseAllocator&) = delete;
			BaseAllocator(const BaseAllocator&&) = delete;
			BaseAllocator& operator =(const BaseAllocator&) = delete;
			BaseAllocator& operator =(BaseAllocator&&) = delete;

			struct AllocationLog
			{
				AllocationLog* prev; //8 bytes
				void* front; //16 bytes 
				void* back; //24 bytes
				const char* file; //32 bytes
				int line; //36 bytes
				//maybe not safe due to possibly allocating more then 4 gb.
				uint32_t alloc_size; //40 bytes
				const char* tagName; //48 bytes
				bool is_array; //52 bytes
			}* front_log = nullptr;
			const char* name;

		protected:
			//Validate the allocator by cheaking for leaks and boundry writes.
			void Validate() const;
		};

		struct LinearAllocator : public BaseAllocator
		{
			LinearAllocator(const size_t a_size, const char* a_name = "unnamed");
			~LinearAllocator();

			operator Allocator();

			//just delete these for safety, copies might cause errors.
			LinearAllocator(const LinearAllocator&) = delete;
			LinearAllocator(const LinearAllocator&&) = delete;
			LinearAllocator& operator =(const LinearAllocator&) = delete;
			LinearAllocator& operator =(LinearAllocator&&) = delete;

			void* Alloc(size_t a_size, size_t a_alignment);
			void Free(void*);
			void Clear();

		private:
			void* m_start;
			void* m_buffer;
			uintptr_t m_end;
		};

		struct FixedLinearAllocator : public BaseAllocator
		{
			FixedLinearAllocator(const size_t a_size, const char* a_Name = "unnamed");
			~FixedLinearAllocator();

			operator Allocator();

			//just delete these for safety, copies might cause errors.
			FixedLinearAllocator(const FixedLinearAllocator&) = delete;
			FixedLinearAllocator(const FixedLinearAllocator&&) = delete;
			FixedLinearAllocator& operator =(const FixedLinearAllocator&) = delete;
			FixedLinearAllocator& operator =(FixedLinearAllocator&&) = delete;

			void* Alloc(size_t a_size, size_t a_alignment);
			void Free(void*);
			void Clear();

		private:
			void* m_start;
			void* m_Buffer;
#ifdef _DEBUG
			uintptr_t m_End;
#endif //_DEBUG
		};

		using StackMarker = uintptr_t;

		struct StackAllocator : public BaseAllocator
		{
			StackAllocator(const size_t a_size, const char* a_name = "unnamed");
			~StackAllocator();

			operator Allocator();

			//just delete these for safety, copies might cause errors.
			StackAllocator(const StackAllocator&) = delete;
			StackAllocator(const StackAllocator&&) = delete;
			StackAllocator& operator =(const StackAllocator&) = delete;
			StackAllocator& operator =(StackAllocator&&) = delete;

			void* Alloc(size_t a_size, size_t a_alignment);
			void Free(void*);
			void Clear();

			void SetMarker(const StackMarker a_pos);
			StackMarker GetMarker()
			{
				return reinterpret_cast<StackMarker>(m_buffer);
			}

		private:
			void* m_start;
			void* m_buffer;
			uintptr_t m_end;
		};

		struct FreelistAllocator : public BaseAllocator
		{
			FreelistAllocator(const size_t a_size, const char* a_Name = "unnamed");
			~FreelistAllocator();

			operator Allocator();

			//just delete these for safety, copies might cause errors.
			FreelistAllocator(const FreelistAllocator&) = delete;
			FreelistAllocator(const FreelistAllocator&&) = delete;
			FreelistAllocator& operator =(const FreelistAllocator&) = delete;
			FreelistAllocator& operator =(FreelistAllocator&&) = delete;

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

			uint8_t* m_start = nullptr;
			FreeBlock* m_FreeBlocks;
			size_t m_Totalalloc_size;
		};

		struct POW_FreelistAllocator : public BaseAllocator
		{
			POW_FreelistAllocator(const size_t, const char* a_Name = "unnamed");
			~POW_FreelistAllocator();

			operator Allocator();

			//just delete these for safety, copies might cause errors.
			POW_FreelistAllocator(const POW_FreelistAllocator&) = delete;
			POW_FreelistAllocator(const POW_FreelistAllocator&&) = delete;
			POW_FreelistAllocator& operator =(const POW_FreelistAllocator&) = delete;
			POW_FreelistAllocator& operator =(POW_FreelistAllocator&&) = delete;

			void* Alloc(size_t a_size, size_t);
			void Free(void* a_ptr);
			void Clear();

			struct FreeBlock
			{
				size_t size;
				FreeBlock* next;
			};

			struct FreeList
			{
				size_t alloc_size;
				size_t fullSize;
				void* start;
				FreeBlock* freeBlock;
			};

			struct AllocHeader
			{
				FreeList* freeList;
			};

			FreeList* m_FreeLists;
			size_t m_FreeBlocksAmount;
		};

		//struct PoolAllocator
		//{
		//	PoolAllocator(const size_t a_objectSize, const size_t a_objectCount, const size_t a_alignment);
		//	~PoolAllocator();

		//	//just delete these for safety, copies might cause errors.
		//	PoolAllocator(const PoolAllocator&) = delete;
		//	PoolAllocator(const PoolAllocator&&) = delete;
		//	PoolAllocator& operator =(const PoolAllocator&) = delete;
		//	PoolAllocator& operator =(PoolAllocator&&) = delete;

		//	void* Alloc(size_t a_size, size_t);
		//	void Free(void* a_ptr);
		//	void Clear();

		//	size_t m_Alignment;
		//	size_t m_ObjectCount;
		//	void** m_start = nullptr;
		//	void** m_Pool;
		//};
	}
}



//inline void* operator new(size_t a_Bytes, BB::memory::LinearAllocator* a_allocator)
//{
//	return a_allocator->alloc(a_Bytes);
//};
