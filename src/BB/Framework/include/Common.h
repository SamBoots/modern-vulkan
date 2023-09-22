#pragma once
#include <cstdint>

namespace BB
{
	namespace allocators
	{
		struct LinearAllocator;
		struct FixedLinearAllocator;
		struct StackAllocator;
		struct FreelistAllocator;
		struct POW_FreelistAllocator;
	}

	//legacy code still used this, so we will just remain using this.
	using LinearAllocator_t = allocators::LinearAllocator;
	using FixedLinearAllocator_t = allocators::FixedLinearAllocator;
	using StackAllocator_t = allocators::StackAllocator;
	using FreelistAllocator_t = allocators::FreelistAllocator;
	using POW_FreelistAllocator_t = allocators::POW_FreelistAllocator;

	class BBImage;

#define BB_CONCAT(a, b) a##b
#define BB_PAD(n) unsigned char BB_CONCAT(_padding_, __LINE__)[n]

//Thank you Descent Raytracer teammates great code that I can steal
#define BB_SLL_PUSH(head, node) ((node)->next = (head), (head) = (node))
#define BB_SLL_POP(head) head; do { (head) = (head)->next; } while(0)

	constexpr const uint64_t BB_INVALID_HANDLE = 0;

	template<typename Tag>
	union FrameworkHandle
	{
		FrameworkHandle() {};
		FrameworkHandle(uint64_t a_handle)
		{
			handle = a_handle;
		};
		FrameworkHandle(uint32_t a_Index, uint32_t a_extra_index)
		{
			index = a_Index;
			extra_index = a_extra_index;
		};
		struct
		{
			//The handle's main index. Always used and is the main handle.
			uint32_t index;
			//A extra handle index, can be used to track something else. Usually this value is 0 or is part of a pointer.
			uint32_t extra_index;
		};
		//Some handles work with pointers.
		void* ptr_handle;
		uint64_t handle{};

		inline bool operator ==(FrameworkHandle a_rhs) const { return handle == a_rhs.handle; }
		inline bool operator !=(FrameworkHandle a_rhs) const { return handle != a_rhs.handle; }
	};

	using WindowHandle = FrameworkHandle<struct WindowHandleTag>;
	//A handle to a loaded lib/dll from OS::LoadLib and can be destroyed using OS::UnloadLib
	using LibHandle = FrameworkHandle<struct LibHandleTag>;
	using OSFileHandle = FrameworkHandle<struct OSFileHandleTag>;
	using BBMutex = FrameworkHandle<struct BBMutexTag>;
	using BBSemaphore = FrameworkHandle<struct BBSemaphoreTag>;
	using BBRWLock = FrameworkHandle<struct BBRWLockTag>;
	using BBConditionalVariable = FrameworkHandle<struct BBConditionalVariableTag>;
	using ThreadTask = FrameworkHandle<struct ThreadTasktag>;

	using wchar = wchar_t;

	struct Buffer
	{
		void* data;
		uint64_t size;
	};


#ifdef _DEBUG
#define BB_MEMORY_DEBUG const char* a_File, int a_Line,
#define BB_MEMORY_DEBUG_ARGS __FILE__, __LINE__,
#define BB_MEMORY_DEBUG_SEND a_File, a_Line,
#define BB_MEMORY_DEBUG_FREE nullptr, 0,
#else //No debug
#define BB_MEMORY_DEBUG 
#define BB_MEMORY_DEBUG_ARGS
#define BB_MEMORY_DEBUG_SEND
#define BB_MEMORY_DEBUG_FREE
#endif //_DEBUG
	typedef void* (*AllocateFunc)(BB_MEMORY_DEBUG void* a_allocator, size_t a_size, const size_t a_Alignment, void* a_OldPtr);
	struct Allocator
	{
		AllocateFunc func;
		void* allocator;
	};


	union float2
	{
		float e[2];
		struct
		{
			float x;
			float y;
		};
	};

	union float3
	{
		float e[3];
		struct
		{
			float x;
			float y;
			float z;
		};
	};

	union float4
	{
		float e[4];
		struct
		{
			float x;
			float y;
			float z;
			float w;
		};
	};

	union int2
	{
		int e[2];
		struct
		{
			int x;
			int y;
		};
	};

	union int3
	{
		int e[3];
		struct
		{
			int x;
			int y;
			int z;
		};
	};

	union int4
	{
		int e[4];
		struct
		{
			int x;
			int y;
			int z;
			int w;
		};
	};

	union uint2
	{
		uint32_t e[2];
		struct
		{
			uint32_t x;
			uint32_t y;
		};
	};

	union uint3
	{
		uint32_t e[3];
		struct
		{
			uint32_t x;
			uint32_t y;
			uint32_t z;
		};
	};

	union uint4
	{
		uint32_t e[4];
		struct
		{
			uint32_t x;
			uint32_t y;
			uint32_t z;
			uint32_t w;
		};
	};

	union Quat
	{
		struct
		{
			float x;
			float y;
			float z;
			float w;
		};
		float3 xyz;
		float4 xyzw;
	};

	union Mat3x3
	{
		float e[3][3];
		struct
		{
			float3 r0;
			float3 r1;
			float3 r2;
		};
	};

	union Mat4x4
	{
		float e[4][4];
		struct
		{
			float4 r0;
			float4 r1;
			float4 r2;
			float4 r3;
		};
	};
}