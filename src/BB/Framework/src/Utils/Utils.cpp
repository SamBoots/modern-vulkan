#include "Utils.h"
#include "Math.inl"

#include <immintrin.h>


namespace BB
{
	void BB::Memory::MemCpy(void* __restrict a_destination, const void* __restrict  a_source, size_t a_size)
	{
		const size_t diff = Pointer::AlignForwardAdjustment(a_destination, sizeof(size_t));

		uint8_t* __restrict align_dest = reinterpret_cast<uint8_t*>(a_destination);
		const uint8_t* __restrict align_src = reinterpret_cast<const uint8_t*>(a_source);

		for (size_t i = 0; i < diff; i++)
		{
			*align_dest++ = *align_src++;
		}

		a_size -= diff;

		//Get the registry size for most optimal memcpy.
		size_t* __restrict  dest = reinterpret_cast<size_t*>(align_dest);
		const size_t* __restrict  src = reinterpret_cast<const size_t*>(align_src);

		while (a_size >= sizeof(size_t))
		{
			*dest++ = *src++;
			a_size -= sizeof(size_t);
		}

		uint8_t* __restrict dest_char = reinterpret_cast<uint8_t*>(dest);
		const uint8_t* __restrict src_char = reinterpret_cast<const uint8_t*>(src);

		//Again but then go by byte.
		while (a_size--)
		{
			*dest_char++ = *src_char++;
		}
	}

	void BB::Memory::MemCpySIMD128(void* __restrict a_destination, const void* __restrict a_source, size_t a_size)
	{
		const size_t diff = Pointer::AlignForwardAdjustment(a_destination, sizeof(__m128i));

		uint8_t* __restrict align_dest = reinterpret_cast<uint8_t*>(a_destination);
		const uint8_t* __restrict align_src = reinterpret_cast<const uint8_t*>(a_source);

		for (size_t i = 0; i < diff; i++)
		{
			*align_dest++ = *align_src++;
		}

		a_size -= diff;

		//Get the registry size for most optimal memcpy.
		__m128i* __restrict  dest = reinterpret_cast<__m128i*>(align_dest);
		const __m128i* __restrict  src = reinterpret_cast<const __m128i*>(align_src);

		while(a_size >= sizeof(__m128i))
		{
			*dest++ = _mm_loadl_epi64(src++);
			a_size -= sizeof(__m128i);
		}

		uint8_t* __restrict dest_char = reinterpret_cast<uint8_t*>(dest);
		const uint8_t* __restrict src_char = reinterpret_cast<const uint8_t*>(src);

		//Again but then go by byte.
		while(a_size--)
		{
			*dest_char++ = *src_char++;
		}
	}

	void BB::Memory::MemCpySIMD256(void* __restrict a_destination, const void* __restrict  a_source, size_t a_size)
	{
		const size_t diff = Pointer::AlignForwardAdjustment(a_destination, sizeof(__m256i));

		uint8_t* __restrict align_dest = reinterpret_cast<uint8_t*>(a_destination);
		const uint8_t* __restrict align_src = reinterpret_cast<const uint8_t*>(a_source);

		for (size_t i = 0; i < diff; i++)
		{
			*align_dest++ = *align_src++;
		}

		a_size -= diff;

		//Get the registry size for most optimal memcpy.
		__m256i* __restrict  dest = reinterpret_cast<__m256i*>(align_dest);
		const __m256i* __restrict  src = reinterpret_cast<const __m256i*>(align_src);

		while (a_size >= sizeof(__m256i))
		{
			*dest++ = _mm256_loadu_si256(src++);
			a_size -= sizeof(__m256i);
		}

		uint8_t* __restrict dest_char = reinterpret_cast<uint8_t*>(dest);
		const uint8_t* __restrict src_char = reinterpret_cast<const uint8_t*>(src);

		//Again but then go by byte.
		while (a_size--)
		{
			*dest_char++ = *src_char++;
		}
	}

	void BB::Memory::MemSet(void* __restrict a_destination, const int32_t a_value, size_t a_size)
	{
		//Get the registry size for most optimal memcpy.
		size_t* __restrict  dest = reinterpret_cast<size_t*>(a_destination);

		while (a_size >= sizeof(size_t))
		{
			*dest++ = a_value;
			a_size -= sizeof(size_t);
		}

		uint8_t* __restrict dest_char = reinterpret_cast<uint8_t*>(dest);

		//Again but then go by byte.
		for (size_t i = 0; i < a_size; i++)
		{
			*dest_char++ = reinterpret_cast<uint8_t*>(static_cast<uint8_t>(a_value))[i];
		}
	}

	void BB::Memory::MemSetSIMD128(void* __restrict  a_destination, const int32_t a_value, size_t a_size)
	{
		//Get the registry size for most optimal memcpy.
		__m128i* __restrict  dest = reinterpret_cast<__m128i*>(a_destination);

		while (a_size >= sizeof(__m128i))
		{
			*dest++ = _mm_set1_epi32(a_value);
			a_size -= sizeof(__m128i);
		}

		size_t* __restrict  t_intDest = reinterpret_cast<size_t*>(dest);

		while (a_size >= sizeof(size_t))
		{
			*t_intDest++ = a_value;
			a_size -= sizeof(size_t);
		}

		uint8_t* __restrict dest_char = reinterpret_cast<uint8_t*>(t_intDest);

		//Again but then go by byte.
		for (size_t i = 0; i < a_size; i++)
		{
			*dest_char++ = reinterpret_cast<uint8_t*>(static_cast<uint8_t>(a_value))[i];
		}
	}

	void BB::Memory::MemSetSIMD256(void* __restrict a_destination, const int32_t a_value, size_t a_size)
	{
		//Get the registry size for most optimal memcpy.
		__m256i* __restrict  dest = reinterpret_cast<__m256i*>(a_destination);

		while (a_size > sizeof(__m256i))
		{
			*dest++ = _mm256_set1_epi32(a_value);
			a_size -= sizeof(__m256i);
		}
		
		size_t* __restrict  t_intDest = reinterpret_cast<size_t*>(dest);

		while (a_size >= sizeof(size_t))
		{
			*t_intDest++ = a_value;
			a_size -= sizeof(size_t);
		}

		uint8_t* __restrict dest_char = reinterpret_cast<uint8_t*>(t_intDest);

		//Again but then go by byte.
		for (size_t i = 0; i < a_size; i++)
		{
			*dest_char++ = reinterpret_cast<uint8_t*>(static_cast<uint8_t>(a_value))[i];
		}
	}

	bool BB::Memory::MemCmp(const void* __restrict a_left, const void* __restrict a_right, size_t a_size)
	{
		const size_t diff = Max(
			Pointer::AlignForwardAdjustment(a_left, sizeof(size_t)),
			Pointer::AlignForwardAdjustment(a_right, sizeof(size_t)));

		const uint8_t* __restrict t_AlignLeft = reinterpret_cast<const uint8_t*>(a_left);
		const uint8_t* __restrict t_AlignRight = reinterpret_cast<const uint8_t*>(a_right);

		for (size_t i = 0; i < diff; i++)
		{
			if (t_AlignLeft++ != t_AlignRight++)
				return false;
		}

		a_size -= diff;

		//Get the registry size for most optimal memcpy.
		const size_t* __restrict  t_Left = reinterpret_cast<const size_t*>(t_AlignLeft);
		const size_t* __restrict  t_Right = reinterpret_cast<const size_t*>(t_AlignRight);

		while (a_size > sizeof(size_t))
		{
			if (t_Left++ != t_Right++)
				return false;
			a_size -= sizeof(size_t);
		}

		const uint8_t* __restrict  t_CharLeft = reinterpret_cast<const uint8_t*>(t_Right);
		const uint8_t* __restrict  t_CharRight = reinterpret_cast<const uint8_t*>(t_Left);

		while (a_size--)
		{
			if (t_CharLeft++ != t_CharRight++)
				return false;
		}

		return true;
	}

	bool BB::Memory::MemCmpSIMD128(const void* __restrict a_left, const void* __restrict a_right, size_t a_size)
	{
		const size_t diff = Max(
			Pointer::AlignForwardAdjustment(a_left, sizeof(__m128i)),
			Pointer::AlignForwardAdjustment(a_right, sizeof(__m128i)));

		const uint8_t* __restrict t_AlignLeft = reinterpret_cast<const uint8_t*>(a_left);
		const uint8_t* __restrict t_AlignRight = reinterpret_cast<const uint8_t*>(a_right);

		for (size_t i = 0; i < diff; i++)
		{
			if (t_AlignLeft++ != t_AlignRight++)
				return false;
		}

		a_size -= diff;

		//Get the registry size for most optimal memcpy.
		const __m128i* __restrict  t_Left = reinterpret_cast<const __m128i*>(t_AlignLeft);
		const __m128i* __restrict  t_Right = reinterpret_cast<const __m128i*>(t_AlignRight);

		const uint64_t t_CmpMode = _SIDD_UBYTE_OPS | _SIDD_CMP_EQUAL_EACH | _SIDD_NEGATIVE_POLARITY | _SIDD_LEAST_SIGNIFICANT;

		while (a_size >= sizeof(__m128i))
		{
			__m128i loadLeft = _mm_loadu_si128(t_Left++);
			__m128i loadRight = _mm_loadu_si128(t_Right++);
			if (_mm_cmpestrc(loadLeft, static_cast<int>(a_size), loadRight, static_cast<int>(a_size), static_cast<int>(t_CmpMode)))
			{
				return false;
			}
			a_size -= sizeof(__m128i);
		}

		const uint8_t* __restrict  t_CharLeft = reinterpret_cast<const uint8_t*>(t_Right);
		const uint8_t* __restrict  t_CharRight = reinterpret_cast<const uint8_t*>(t_Left);

		while (a_size--)
		{
			if (t_CharLeft++ != t_CharRight++)
				return false;
		}

		return true;
	}

	bool BB::Memory::MemCmpSIMD256(const void* __restrict a_left, const void* __restrict a_right, size_t a_size)
	{
		const size_t diff = Max(
			Pointer::AlignForwardAdjustment(a_left, sizeof(__m256i)),
			Pointer::AlignForwardAdjustment(a_right, sizeof(__m256i)));

		const uint8_t* __restrict t_AlignLeft = reinterpret_cast<const uint8_t*>(a_left);
		const uint8_t* __restrict t_AlignRight = reinterpret_cast<const uint8_t*>(a_right);

		for (size_t i = 0; i < diff; i++)
		{
			if (t_AlignLeft++ != t_AlignRight++)
				return false;
		}

		a_size -= diff;

		const __m256i* __restrict t_Left = reinterpret_cast<const __m256i*>(t_AlignLeft);
		const __m256i* __restrict t_Right = reinterpret_cast<const __m256i*>(t_AlignRight);
		
		for (/**/; a_size >= sizeof(__m256i); t_Left++, t_Right++)
		{
			const __m256i loadLeft = _mm256_loadu_si256(t_Left);
			const __m256i loadRight = _mm256_loadu_si256(t_Right);
			const __m256i result = _mm256_cmpeq_epi64(loadLeft, loadRight);
			if (!(unsigned int)_mm256_testc_si256(result, _mm256_set1_epi64x(0xFFFFFFFFFFFFFFFF)))
			{
				return false;
			}
			a_size -= sizeof(__m256i);
		}

		const uint8_t* __restrict  t_CharLeft = reinterpret_cast<const uint8_t*>(t_Right);
		const uint8_t* __restrict  t_CharRight = reinterpret_cast<const uint8_t*>(t_Left);

		while (a_size--)
		{
			if (t_CharLeft++ != t_CharRight++)
				return false;
		}

		return true;
	}
}