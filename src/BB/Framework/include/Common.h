#pragma once
#include <cstdint>
#include <BBIntrin.h>

namespace BB
{
#define BB_CONCAT(a, b) a##b

#define BB_PRAGMA(X)			_Pragma(#X)
#define BB_PRAGMA_PACK_PUSH(n)  BB_PRAGMA(pack(push,n))
#define BB_PRAGMA_PACK_POP()    BB_PRAGMA(pack(pop))
#ifdef __clang__
#define BB_PAD(n)   BB_PRAGMA(clang diagnostic push) \
					BB_PRAGMA(clang diagnostic ignored "-Wunused-private-field") \
	                unsigned char BB_CONCAT(_padding_, __LINE__)[n] \
					BB_PRAGMA(clang diagnostic pop)

#define BB_NO_RETURN			__attribute__((noreturn))

#define BB_WARNINGS_OFF			BB_PRAGMA(clang diagnostic push) \
								BB_PRAGMA(clang diagnostic ignored "-Wall")	\
								BB_PRAGMA(clang diagnostic ignored "-Wextra") \
								BB_PRAGMA(clang diagnostic ignored "-Weverything") \
								BB_PRAGMA(clang diagnostic ignored "-Wpedantic")

#define BB_WARNINGS_ON			BB_PRAGMA(clang diagnostic pop)
#elif _MSC_VER
#define BB_PAD(n) unsigned char BB_CONCAT(_padding_, __LINE__)[n]

#define BB_NO_RETURN			__declspec(noreturn)

#define BB_WARNINGS_OFF			BB_PRAGMA(warning(push, 0))
#define BB_WARNINGS_ON			BB_PRAGMA(warning(pop, 0))
#endif

	// logger info 
	using WarningTypeFlags = unsigned int;
	enum class WarningType : WarningTypeFlags
	{
		INFO = 1 << 0, //No warning, just a message.
		OPTIMALIZATION = 1 << 1, //Indicates a possible issue that might cause problems with performance.
		LOW = 1 << 2, //Low chance of breaking the application or causing undefined behaviour.
		MEDIUM = 1 << 3, //Medium chance of breaking the application or causing undefined behaviour.
		HIGH = 1 << 4, //High chance of breaking the application or causing undefined behaviour.
		ASSERT = 1 << 5  //Use BB_ASSERT for this
	};
	constexpr WarningTypeFlags WARNING_TYPES_ALL = UINT32_MAX;
	
	// memory info
	constexpr const size_t kbSize = 1024;
	constexpr const size_t mbSize = kbSize * 1024;
	constexpr const size_t gbSize = mbSize * 1024;

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

	constexpr const uint64_t BB_INVALID_HANDLE_64 = UINT64_MAX;

	template<typename Tag>
	union FrameworkHandle
	{
		constexpr FrameworkHandle() : handle(BB_INVALID_HANDLE_64) {}
		constexpr explicit FrameworkHandle(const uint64_t a_handle) : handle(a_handle) {}
		constexpr explicit FrameworkHandle(const uint32_t a_index, const uint32_t a_extra_index) : index(a_index), extra_index(a_extra_index) {}

		bool IsValid() const { return handle != BB_INVALID_HANDLE_64; }
		struct
		{
			//The handle's main index. Always used and is the main handle.
			uint32_t index;
			//A extra handle index, can be used to track something else. Usually this value is 0 or is part of a pointer.
			uint32_t extra_index;
		};
		uint64_t handle;

		inline bool operator ==(const FrameworkHandle a_rhs) const { return handle == a_rhs.handle; }
		inline bool operator !=(const FrameworkHandle a_rhs) const { return handle != a_rhs.handle; }
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

	constexpr const uint32_t BB_INVALID_HANDLE_32 = UINT32_MAX;

	template<typename Tag>
	struct FrameworkHandle32Bit
	{
		constexpr FrameworkHandle32Bit() : handle(BB_INVALID_HANDLE_32) {}
		constexpr explicit FrameworkHandle32Bit(const uint32_t a_handle) : handle(a_handle) {}

		uint32_t handle;

		bool IsValid() const { return handle != BB_INVALID_HANDLE_32; }

		inline bool operator ==(const FrameworkHandle32Bit a_rhs) const { return handle == a_rhs.handle; }
		inline bool operator !=(const FrameworkHandle32Bit a_rhs) const { return handle != a_rhs.handle; }
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
#define BB_MEMORY_DEBUG_VOID_ARRAY
#define BB_MEMORY_DEBUG_UNUSED
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
		constexpr float2() : x(0), y(0) {}
		constexpr float2(const float a_value) : x(a_value), y(a_value) {}
		constexpr float2(const float a_x, const float a_y) : x(a_x), y(a_y) {}
		float e[2];
		struct
		{
			float x;
			float y;
		};
	};

	union float3
	{
		constexpr float3() : x(0), y(0), z(0) {}
		constexpr float3(const float a_value) : x(a_value), y(a_value), z(a_value) {}
		constexpr float3(const float a_x, const float a_y, const float a_z) : x(a_x), y(a_y), z(a_z) {}
		float e[3];
		struct
		{
			float x;
			float y;
			float z;
		};
	};

	//no constexpr constructors here, assembly ftw
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
		constexpr int2() : x(0), y(0) {}
		constexpr int2(const int a_x, const int a_y) : x(a_x), y(a_y) {}
		int e[2];
		struct
		{
			int x;
			int y;
		};
	};

	union int3
	{
		constexpr int3() : x(0), y(0), z(0) {}
		constexpr int3(const int a_x, const int a_y, const int a_z) : x(a_x), y(a_y), z(a_z) {}
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
		constexpr int4() : x(0), y(0) {}
		constexpr int4(const int a_x, const int a_y, const int a_z, const int a_w) : x(a_x), y(a_y), z(a_z), w(a_w) {}
		constexpr int4(const VecInt4 a_vec) : vec(a_vec) {}
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
		constexpr uint2() : x(0), y(0) {}
		constexpr uint2(const unsigned int a_value) : x(a_value), y(a_value) {}
		constexpr uint2(const unsigned int a_x, const unsigned int a_y) : x(a_x), y(a_y) {}
		unsigned int e[2];
		struct
		{
			unsigned int x;
			unsigned int y;
		};
	};

	static inline bool operator==(const uint2 a_lhs, const uint2 a_rhs)
	{
		if (a_lhs.x == a_rhs.x && a_lhs.y == a_rhs.y)
			return true;
		return false;
	}

	static inline bool operator!=(const uint2 a_lhs, const uint2 a_rhs)
	{
		if (a_lhs.x != a_rhs.x || a_lhs.y != a_rhs.y)
			return true;
		return false;
	}

	union uint3
	{
		constexpr uint3() : x(0), y(0), z(0) {}
		constexpr uint3(const unsigned int a_value) : x(a_value), y(a_value), z(a_value) {}
		constexpr uint3(const unsigned int a_x, const unsigned int a_y, const unsigned int a_z) : x(a_x), y(a_y), z(a_z) {}
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
