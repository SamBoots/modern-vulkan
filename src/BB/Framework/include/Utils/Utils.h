#pragma once
#include <cstdint>
#include <cmath>
#include <cstring>

#include <cwchar>

#include <immintrin.h>

namespace BB
{
	namespace Memory
	{

		void MemCpy(void* __restrict  a_destination, const void* __restrict  a_source, size_t a_size);
		void MemCpySIMD128(void* __restrict  a_destination, const void* __restrict  a_source, size_t a_size);
		void MemCpySIMD256(void* __restrict  a_destination, const void* __restrict  a_source, size_t a_size);

		void MemSet(void* __restrict  a_destination, const size_t a_value, size_t a_size);
		void MemSetSIMD128(void* __restrict a_destination, const size_t a_value, size_t a_size);
		void MemSetSIMD256(void* __restrict  a_destination, const size_t a_value, size_t a_size);

		bool MemCmp(const void* __restrict  a_left, const void* __restrict  a_right, size_t a_size);
		bool MemCmpSIMD128(const void* __restrict  a_left, const void* __restrict  a_right, size_t a_size);
		bool MemCmpSIMD256(const void* __restrict  a_left, const void* __restrict  a_right, size_t a_size);


		/// <summary>
		/// Memcpy abstraction that will call the constructor if needed.
		/// </summary>
		template<typename T>
		inline static void Copy(T* __restrict a_destination, const T* __restrict a_source, const size_t a_element_count)
		{
			if constexpr (std::is_trivially_constructible_v<T>)
			{
				memcpy(a_destination, a_source, a_element_count * sizeof(T));
			}
			else
			{
				for (size_t i = 0; i < a_element_count; i++)
				{
					new (&a_destination[i]) T(a_source[i]);
				}
			}
		}

		/// <summary>
		/// Memcpy abstraction that will call the constructor if needed.
		/// </summary>
		template<typename T>
		inline static void Copy(void* __restrict a_destination, const T* __restrict a_source, const size_t a_element_count)
		{
			Memory::Copy(reinterpret_cast<T * __restrict>(a_destination), a_source, a_element_count);
		}

		/// <summary>
		/// Memcpy abstraction that will call the constructor if needed.
		/// </summary>
		template<typename T>
		inline static void Copy(T* __restrict a_destination, const void* __restrict a_source, const size_t a_element_count)
		{
			Memory::Copy(a_destination, reinterpret_cast<const T * __restrict>(a_source), a_element_count);
		}

		/// <summary>
		/// Memmove abstraction that will call the constructor and/or deconstructor if needed.
		/// </summary>
		template<typename T>
		inline static void* Move(T* __restrict a_destination, const T* __restrict a_source, const size_t a_element_count)
		{
			constexpr bool TRIVIAL_CONTSRUCTABLE = std::is_trivially_constructible_v<T>;
			constexpr bool TRIVIAL_DESTRUCTABLE = std::is_trivially_destructible_v<T>;

			if constexpr (TRIVIAL_CONTSRUCTABLE)
			{
				return memmove(a_destination, a_source, a_element_count * sizeof(T));
			}
			else if constexpr (!TRIVIAL_CONTSRUCTABLE && !TRIVIAL_DESTRUCTABLE)
			{
				for (size_t i = 0; i < a_element_count; i++)
				{
					if constexpr (!TRIVIAL_CONTSRUCTABLE)
					{
						new (&a_destination[i]) T(a_source[i]);
					}
					if constexpr (!TRIVIAL_DESTRUCTABLE)
					{
						a_source[i].~T();
					}
				}
				return a_destination;
			}
			else
				BB_STATIC_ASSERT(false, "Something weird happened, Unsafe Move.");
			return nullptr;
		}

		/// <summary>
		/// memset abstraction that will use the sizeof operator for type T.
		/// </summary>
		template<typename T>
		inline static void* Set(T* __restrict a_destination, const int a_value, const size_t a_element_count)
		{
			return memset(a_destination, a_value, a_element_count * sizeof(T));
		}

		/// <summary>
		/// memcmp abstraction that will use the sizeof operator for type T.
		/// </summary>
		template<typename T>
		inline static int Compare(const T* __restrict a_left, const void* __restrict a_right, const size_t a_element_count)
		{
			return memcmp(a_left, a_right, a_element_count * sizeof(T));
		}

		inline static size_t StrLength(const char* a_string)
		{
			return strlen(a_string);
		}

		inline static size_t StrLength(const wchar_t* a_string)
		{
			return wcslen(a_string);
		}
	}

	inline static size_t RoundUp(const size_t a_round_num, const size_t a_multiple)
	{
		return ((a_round_num + a_multiple - 1) / a_multiple) * a_multiple;
	}

	inline static size_t Max(const size_t a_a, const size_t a_b)
	{
		if (a_a > a_b)
			return a_a;
		return a_b;
	}

	inline static void* Max(const void* a_a, const void* a_b)
	{
		return reinterpret_cast<void*>(Max(reinterpret_cast<size_t>(a_a), reinterpret_cast<size_t>(a_b)));
	}

	inline static size_t Min(const size_t a_a, const size_t a_b)
	{
		if (a_a < a_b)
			return a_a;
		return a_b;
	}

	inline static int Max(const int a_a, const int a_b)
	{
		if (a_a > a_b)
			return a_a;
		return a_b;
	}

	inline static int Min(const int a_a, const int a_b)
	{
		if (a_a < a_b)
			return a_a;
		return a_b;
	}

	inline static float Maxf(const float a_a, const float a_b)
	{
		if (a_a > a_b)
			return a_a;
		return a_b;
	}

	inline static float Minf(const float a_a, const float a_b)
	{
		if (a_a < a_b)
			return a_a;
		return a_b;
	}

	inline static float Lerp(const float a_a, const float a_b, const float a_t)
	{
		return a_a + a_t * (a_b - a_a);
	}

	inline static int Clamp(const int a_value, const int a_min, const int a_max)
	{
		const int i = a_value < a_min ? a_max : a_value;
		return i > a_max ? a_max : i;
	}

	inline static uint32_t Clamp(const uint32_t a_value, const uint32_t a_min, const uint32_t a_max)
	{
		const uint32_t i = a_value < a_min ? a_max : a_value;
		return i > a_max ? a_max : i;
	}

	inline static float Clampf(const float a_value, const float a_min, const float a_max)
	{
		const float f = a_value < a_min ? a_max : a_value;
		return f > a_max ? a_max : f;
	}

	namespace Random
	{
		static unsigned int shit_code_math_random_seed = 1;

		/// <summary>
		/// Set the random seed that the Math.h header uses.
		/// </summary>
		inline static void Seed(const unsigned int a_seed)
		{
			shit_code_math_random_seed = a_seed;
		}

		/// <summary>
		/// Get a Random unsigned int between 0 and INT_MAX.
		/// </summary>
		inline static unsigned int Random()
		{
			shit_code_math_random_seed ^= shit_code_math_random_seed << 13;
			shit_code_math_random_seed ^= shit_code_math_random_seed >> 17;
			shit_code_math_random_seed ^= shit_code_math_random_seed << 5;
			return shit_code_math_random_seed;
		}

		/// <summary>
		/// Get a Random unsigned int between 0 and maxValue.
		/// </summary>
		inline static  unsigned int Random(const unsigned int a_max)
		{
			return Random() % a_max;
		}

		/// <summary>
		/// Get a Random unsigned int between min and max value.
		/// </summary>
		inline static unsigned int Random(const unsigned int a_min, const unsigned int a_max)
		{
			return Random() % (a_max + 1 - a_min) + a_min;
		}

		/// <summary>
		/// Get a Random float between 0 and 1.
		/// </summary>
		inline static float RandomF()
		{
			return fmod(static_cast<float>(Random()) * 2.3283064365387e-10f, 1.0f);
		}

		/// <summary>
		/// Get a Random float between 0 and max value.
		/// </summary>
		inline static  float RandomF(const float a_min, const float a_max)
		{
			return (RandomF() * (a_max - a_min)) + a_min;
		}
	}

	namespace Pointer
	{
		/// <summary>
		/// Move the given pointer by a given size.
		/// </summary>
		/// <param name="a_ptr:"> The pointer you want to shift </param>
		/// <param name="a_add:"> The amount of bytes you want move the pointer forward. </param>
		/// <returns>The shifted pointer. </returns>
		inline static void* Add(const void* a_ptr, const size_t a_add)
		{
			return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(a_ptr) + a_add);
		}

		/// <summary>
		/// Move the given pointer by a given size.
		/// </summary>
		/// <param name="a_ptr:"> The pointer you want to shift </param>
		/// <param name="a_subtract:"> The amount of bytes you want move the pointer backwards. </param>
		/// <returns>The shifted pointer. </returns>
		inline static void* Subtract(const void* a_ptr, const size_t a_subtract)
		{
			return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(a_ptr) - a_subtract);
		}

		/// <summary>
		/// Returns a aligned size from a_size based on the a_alignment size given.
		/// </summary>
		/// <param name="a_size:"> Size of the origional buffer </param>
		/// <param name="a_alignment:"> Alignment the returned size needs to be based off. </param>
		/// <returns> an aligned size based of a_size and a_alignment. </returns>
		inline static size_t AlignPad(const size_t a_size, const size_t a_alignment)
		{
			size_t aligned_size = a_size;
			if (a_alignment > 0) {
				aligned_size = (a_size + a_alignment - 1) & ~(a_alignment - 1);
			}
			return aligned_size;
		}

#pragma warning(disable:4146)

		/// <summary>
		/// Align a given pointer forward.
		/// </summary>
		/// <param name="a_ptr:"> The pointer you want to align </param>
		/// <param name="a_alignment:"> The alignment of the data. </param>
		/// <returns>The given address but aligned forward. </returns>
		inline static size_t AlignForwardAdjustment(const void* a_ptr, const size_t a_alignment)
		{
			const uintptr_t uptr = reinterpret_cast<uintptr_t>(a_ptr);
			const uintptr_t aligned_ptr = (uptr - 1u + a_alignment) & -a_alignment;

			return aligned_ptr - uptr;
		}
		/// <summary>
		/// Returns the required forward alignment.
		/// </summary>
		/// <param name="a_value:"> The value you want to align </param>
		/// <param name="a_alignment:"> The alignment of the data. </param>
		/// <returns>The given address but aligned forward. </returns>
		inline static size_t AlignForwardAdjustment(const size_t a_value, const size_t a_alignment)
		{
			const uintptr_t aligned_ptr = (a_value - 1u + a_alignment) & -a_alignment;

			return aligned_ptr - a_value;
		}
#pragma warning(default:4146)
		/// <summary>
		/// Align a given pointer forward.
		/// </summary>
		/// <param name="a_ptr:"> The pointer you want to align </param>
		/// <param name="a_alignment:"> The alignment of the data. </param>
		/// <param name="a_HeaderSize:"> The size in bytes of the Header you want to align forward's too </param>
		/// <returns>The given address but aligned forward with the allocation header's size in mind. </returns>
		inline static size_t AlignForwardAdjustmentHeader(const void* a_ptr, const size_t a_alignment, const size_t a_HeaderSize)
		{
			size_t adjustment = AlignForwardAdjustment(a_ptr, a_alignment);
			size_t needed_space = a_HeaderSize;

			if (adjustment < needed_space)
			{
				needed_space -= adjustment;

				//Increase adjustment to fit header 
				adjustment += a_alignment * (needed_space / a_alignment);

				if (needed_space % a_alignment > 0) adjustment += a_alignment;
			}

			return adjustment;
		}
	}
}
