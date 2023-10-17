#pragma once
#include <intrin.h>

#define BB_USE_SIMD

namespace BB
{
	static inline void BBInterlockedIncrement64(volatile long long* a_value)
	{
		_InterlockedIncrement64(a_value);
	}

#ifdef BB_USE_SIMD
	using VecFloat4 = __m128;
	using VecInt4 = __m128;
	using VecUint4 = __m128;
#else
	struct VecFloat4
	{
		float x, y, z, w;
	};
	struct VecInt4
	{
		int x, y, z, w;
	};
	struct VecUint4
	{
		uint32_t x, y, z, w;
	};
#endif

	static inline VecFloat4 LoadFloat4(const float a_x, const float a_y, const float a_z, const float a_w)
	{
#ifdef BB_USE_SIMD
		return _mm_set_ps(a_x, a_y, a_z, a_w);
#else
		return VecFloat4{ a_x, a_y, a_z, a_w };
#endif
	}

	static inline VecFloat4 AddFloat4(VecFloat4 a_lhs, VecFloat4 a_rhs)
	{
#ifdef BB_USE_SIMD
		return _mm_add_ps(a_lhs, a_rhs);
#else
		return VecFloat4{ a_lhs.x + a_rhs.x, a_lhs.y + a_rhs.y, a_lhs.z + a_rhs.z, a_lhs.w + a_rhs.w };
#endif
	}

	static inline VecFloat4 SubFloat4(VecFloat4 a_lhs, VecFloat4 a_rhs)
	{
#ifdef BB_USE_SIMD
		return _mm_sub_ps(a_lhs, a_rhs);
#else
		return VecFloat4{ a_lhs.x - a_rhs.x, a_lhs.y - a_rhs.y, a_lhs.z - a_rhs.z, a_lhs.w - a_rhs.w };
#endif
	}

	static inline VecFloat4 MulFloat4(VecFloat4 a_lhs, VecFloat4 a_rhs)
	{
#ifdef BB_USE_SIMD
		return _mm_mul_ps(a_lhs, a_rhs);
#else
		return VecFloat4{ a_lhs.x * a_rhs.x, a_lhs.y * a_rhs.y, a_lhs.z * a_rhs.z, a_lhs.w * a_rhs.w };
#endif
	}

	static inline VecFloat4 MulFloat4(VecFloat4 a_vec, float a_f)
	{
#ifdef BB_USE_SIMD
		return _mm_mul_ps(a_vec, _mm_set_ps1(a_f));
#else
		return VecFloat4{ a_vec.x * a_f, a_vec.y * a_f, a_vec.z * a_f, a_vec.w * a_f };
#endif
	}

	static inline VecFloat4 DivFloat4(VecFloat4 a_lhs, VecFloat4 a_rhs)
	{
#ifdef BB_USE_SIMD
		return _mm_div_ps(a_lhs, a_rhs);
#else
		return VecFloat4{ a_lhs.x / a_rhs.x, a_lhs.y / a_rhs.y, a_lhs.z / a_rhs.z, a_lhs.w / a_rhs.w };
#endif
	}
}