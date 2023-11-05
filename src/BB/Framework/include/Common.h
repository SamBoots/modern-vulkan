#pragma once
#include <cstdint>
#include <BBIntrin.h>

namespace BB
{
#if defined(__GNUC__) || defined(__MINGW32__) || defined(__clang__) 
#define BB_NO_RETURN __attribute__((noreturn))
#elif _MSC_VER
#define BB_NO_RETURN __declspec(noreturn)
#endif

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
		FrameworkHandle() {}
		FrameworkHandle(uint64_t a_handle)
		{
			handle = a_handle;
		}
		FrameworkHandle(unsigned int a_index, unsigned int a_extra_index)
		{
			index = a_index;
			extra_index = a_extra_index;
		}
		struct
		{
			//The handle's main index. Always used and is the main handle.
			unsigned int index;
			//A extra handle index, can be used to track something else. Usually this value is 0 or is part of a pointer.
			unsigned int extra_index;
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

	template<typename Tag>
	union FrameworkHandle32Bit
	{
		FrameworkHandle32Bit() {}
		FrameworkHandle32Bit(uint32_t a_handle)
		{
			handle = a_handle;
		}
		uint32_t handle{};

		inline bool operator ==(FrameworkHandle32Bit a_rhs) const { return handle == a_rhs.handle; }
		inline bool operator !=(FrameworkHandle32Bit a_rhs) const { return handle != a_rhs.handle; }
	};

	using wchar = wchar_t;

	struct Buffer
	{
		void* data;
		uint64_t size;
	};


#ifdef _DEBUG
#define BB_MEMORY_DEBUG const char* a_file, int a_line, bool a_is_array,
#define BB_MEMORY_DEBUG_VOID_ARRAY (void)a_is_array
#define BB_MEMORY_DEBUG_UNUSED const char*, int, bool,
#define BB_MEMORY_DEBUG_ARGS __FILE__, __LINE__, false,
#define BB_MEMORY_DEBUG_SEND a_file, a_line, false,
#define BB_MEMORY_DEBUG_FREE nullptr, 0, false,

#define BB_MEMORY_DEBUG_ARGS_ARR __FILE__, __LINE__, true,
#define BB_MEMORY_DEBUG_SEND_ARR a_file, a_line, true,
#define BB_MEMORY_DEBUG_FREE_ARR nullptr, 0, true,
#else //No debug
#define BB_MEMORY_DEBUG 
#define BB_MEMORY_DEBUG_ARGS
#define BB_MEMORY_DEBUG_SEND
#define BB_MEMORY_DEBUG_FREE

#define BB_MEMORY_DEBUG_ARGS_ARR
#define BB_MEMORY_DEBUG_SEND_ARR
#define BB_MEMORY_DEBUG_FREE_ARR
#endif //_DEBUG
	typedef void* (*AllocateFunc)(BB_MEMORY_DEBUG void* a_allocator, size_t a_size, const size_t a_alignment, void* a_old_ptr);
	struct Allocator
	{
		AllocateFunc func;
		void* allocator;
	};


	union float2
	{
		float2() { x = 0; y = 0; }
		float2(const float a_value) { x = a_value; y = a_value; }
		float2(const float a_x, const float a_y) { x = a_x; y = a_y; }
		float e[2];
		struct
		{
			float x;
			float y;
		};
	};

	union float3
	{
		float3() { x = 0; y = 0; z = 0; }
		float3(const float a_value) { x = a_value; y = a_value; z = a_value; }
		float3(const float a_x, const float a_y, const float a_z) { x = a_x; y = a_y; z = a_z; }
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
		float4() { vec = LoadFloat4Zero(); }
		float4(const float a_value) { vec = LoadFloat4(a_value); }
		float4(const float a_x, const float a_y, const float a_z, const float a_w) { vec = LoadFloat4(a_x, a_y, a_z, a_w); }
		float4(const VecFloat4 a_vec) { vec = a_vec; }
		float e[4];
		struct
		{
			float x;
			float y;
			float z;
			float w;
		};
		VecFloat4 vec;
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
		int4() = default;
		int4(const int a_x, const int a_y, const int a_z, const int a_w) { x = a_x; y = a_y; z = a_z; w = a_w; }
		int4(const VecInt4 a_vec) { vec = a_vec; }
		int e[4];
		VecInt4 vec;
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
		unsigned int e[2];
		struct
		{
			unsigned int x;
			unsigned int y;
		};
	};

	union uint3
	{
		unsigned int e[3];
		struct
		{
			unsigned int x;
			unsigned int y;
			unsigned int z;
		};
	};

	union uint4
	{
		uint4() = default;
		uint4(const unsigned int a_x, const unsigned int a_y, const unsigned int a_z, const unsigned int a_w) { x = a_x; y = a_y; z = a_z; w = a_w; }
		uint4(const VecUint4 a_vec) { vec = a_vec; }
		unsigned int e[4];
		struct
		{
			unsigned int x;
			unsigned int y;
			unsigned int z;
			unsigned int w;
		};
		VecUint4 vec;
	};

	union Quat
	{
		Quat() { vec = LoadFloat4Zero(); }
		Quat(const float a_x, const float a_y, const float a_z, const float a_w) { vec = LoadFloat4(a_x, a_y, a_z, a_w); }
		Quat(const VecFloat4 a_vec) { vec = a_vec; }
		struct 
		{
			float x;
			float y;
			float z;
			float w;
		};
		float3 xyz;
		float4 xyzw;
		VecFloat4 vec;
	};

	union float3x3
	{
		float e[3][3];
		struct
		{
			float3 r0;
			float3 r1;
			float3 r2;
		};
	};

	union float4x4
	{
		float4x4()
		{
			r0 = {};
			r1 = {};
			r2 = {};
			r3 = {};
		}
		float e[4][4];
		struct
		{
			float4 r0;
			float4 r1;
			float4 r2;
			float4 r3;
		};
		VecFloat4 vec[4];
	};
}
