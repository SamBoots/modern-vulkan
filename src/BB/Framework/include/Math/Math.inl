#pragma once
#include "Common.h"
#include <cmath>
#include <limits>

namespace BB
{
	constexpr float PI_F = 3.14159265358979323846f;
	constexpr double PI_D = 3.14159265358979323846;
	constexpr float F_EPSILON = 0.000000001f;

	constexpr static inline float ToRadians(const float degrees)
	{
		return degrees * 0.01745329252f;
	}

	constexpr static inline bool FloatEqual(const float a_a, const float a_b, const float a_epsilon = std::numeric_limits<float>::epsilon())
	{
		return fabs(a_a - a_b) < a_epsilon;
	}

	// UTILITY
	//--------------------------------------------------------
	// UINT2

	static inline uint2 operator+(const uint2 a_lhs, const uint2 a_rhs)
	{
		return uint2{ a_lhs.x + a_rhs.x, a_lhs.y + a_rhs.y };
	}

	static inline uint2 operator-(const uint2 a_lhs, const uint2 a_rhs)
	{
		return uint2{ a_lhs.x - a_rhs.x, a_lhs.y - a_rhs.y };
	}

	static inline uint2 operator*(const uint2 a_lhs, const uint2 a_rhs)
	{
		return uint2{ a_lhs.x * a_rhs.x, a_lhs.y * a_rhs.y };
	}
	
	static inline uint2 operator*(const uint2 a_lhs, const unsigned int a_rhs)
	{
		return uint2{ a_lhs.x * a_rhs, a_lhs.y * a_rhs };
	}

	static inline uint2 operator/(const uint2 a_lhs, const uint2 a_rhs)
	{
		return uint2{ a_lhs.x / a_rhs.x, a_lhs.y / a_rhs.y };
	}

	static inline uint2 operator/(const uint2 a_lhs, const unsigned int a_rhs)
	{
		return uint2{ a_lhs.x / a_rhs, a_lhs.y / a_rhs };
	}

	// UINT2
	//--------------------------------------------------------
	// FLOAT2

	static inline float2 operator+(const float2 a_lhs, const float2 a_rhs)
	{
		return float2{ a_lhs.x + a_rhs.x, a_lhs.y + a_rhs.y };
	}

	static inline float2 operator-(const float2 a_lhs, const float2 a_rhs)
	{
		return float2{ a_lhs.x - a_rhs.x, a_lhs.y - a_rhs.y };
	}

	static inline float2 operator*(const float2 a_lhs, const float2 a_rhs)
	{
		return float2{ a_lhs.x * a_rhs.x, a_lhs.y * a_rhs.y };
	}

	static inline float2 operator*(const float2 a_lhs, const float a_rhs)
	{
		return float2{ a_lhs.x * a_rhs, a_lhs.y * a_rhs };
	}

	static inline float2 operator/(const float2 a_lhs, const float2 a_rhs)
	{
		return float2{ a_lhs.x / a_rhs.x, a_lhs.y / a_rhs.y };
	}

	static inline float2 operator/(const float2 a_lhs, const float a_rhs)
	{
		return float2{ a_lhs.x / a_rhs, a_lhs.y / a_rhs };
	}

	// FLOAT2
	//--------------------------------------------------------
	// FLOAT3

	static inline bool operator==(const float3 a_lhs, const float3 a_rhs)
	{
		return FloatEqual(a_lhs.x, a_rhs.x) && FloatEqual(a_lhs.y, a_rhs.y) && FloatEqual(a_lhs.z, a_rhs.z);
	}

	static inline bool operator!=(const float3 a_lhs, const float3 a_rhs)
	{
		return !FloatEqual(a_lhs.x, a_rhs.x) || !FloatEqual(a_lhs.y, a_rhs.y) || !FloatEqual(a_lhs.z, a_rhs.z);
	}

	static inline bool operator<(const float3 a_lhs, const float3 a_rhs)
	{
		return a_lhs.x < a_rhs.x && a_lhs.y < a_rhs.y && a_lhs.z < a_rhs.z;
	}

	static inline bool operator>(const float3 a_lhs, const float3 a_rhs)
	{
		return a_lhs.x > a_rhs.x && a_lhs.y > a_rhs.y && a_lhs.z > a_rhs.z;
	}

	static inline float3 operator+(const float3 a_lhs, const float3 a_rhs)
	{
		return float3{ a_lhs.x + a_rhs.x, a_lhs.y + a_rhs.y, a_lhs.z + a_rhs.z };
	}

	static inline float3 operator-(const float3 a_lhs, const float3 a_rhs)
	{
		return float3{ a_lhs.x - a_rhs.x, a_lhs.y - a_rhs.y, a_lhs.z - a_rhs.z };
	}

	static inline float3 operator*(const float3 a_lhs, const float a_float)
	{
		return float3{ a_lhs.x * a_float, a_lhs.y * a_float, a_lhs.z * a_float };
	}

	static inline float3 operator*(const float3 a_lhs, const float3 a_rhs)
	{
		return float3{ a_lhs.x * a_rhs.x, a_lhs.y * a_rhs.y, a_lhs.z * a_rhs.z };
	}

	static inline float3 operator*(const float3x3 a_mat, const float3 a_vec)
	{
		float3 vec;
		vec.x = a_vec.x * a_mat.r0.x + a_vec.y * a_mat.r1.x + a_vec.z * a_mat.r2.x;
		vec.y = a_vec.x * a_mat.r0.y + a_vec.y * a_mat.r1.y + a_vec.z * a_mat.r2.y;
		vec.z = a_vec.x * a_mat.r0.z + a_vec.y * a_mat.r1.z + a_vec.z * a_mat.r2.z;
		return vec;
	}

	static inline float3 operator/(const float3 a_lhs, const float3 a_rhs)
	{
		return float3{ a_lhs.x / a_rhs.x, a_lhs.y / a_rhs.y, a_lhs.z / a_rhs.z };
	}

	static inline float3 Float3Abs(const float3 a_v)
	{
		return float3(fabs(a_v.x), fabs(a_v.y), fabs(a_v.z));
	}

	static inline float3 Float3Distance(const float3 a_p0, const float3 a_p1)
	{
		return Float3Abs(a_p0 - a_p1);
	}

    static inline float FloatDistance(const float3 a_p0, const float3 a_p1)
    {
        const float3 abs = Float3Abs(a_p0 - a_p1);
        return abs.x + abs.y + abs.z;
    }

	static inline float3 Float3Cross(const float3 a, const float3 b)
	{
		float3 result;
		result.x = a.y * b.z - a.z * b.y;
		result.y = a.z * b.x - a.x * b.z;
		result.z = a.x * b.y - a.y * b.x;
		return result;
	}

	static inline float Float3Dot(const float3 a, const float3 b)
	{
		return a.x * b.x + a.y * b.y + a.z * b.z;
	}

	static inline float Float3LengthSq(const float3 a)
	{
		return Float3Dot(a, a);
	}

	static inline float Float3Length(const float3 a)
	{
		return sqrtf(Float3LengthSq(a));
	}

	static inline float3 Float3Normalize(const float3 a)
	{
		const float length = Float3Length(a);
		const float rcp_length = 1.0f / length;
		return a * rcp_length;
	}

    static inline float3 Float3RotatePoint(const float3x3& a_rotation_matrix, const float3 a_point, const float3 a_middle)
    {
        const float3 res = a_rotation_matrix * (a_point - a_middle);
        return a_middle + res;
    }

	inline static float3 Float3Lerp(const float3 a_p0, const float3 a_p1, const float a_t)
	{
		return a_p0 + (a_p1 - a_p0) * a_t;
	}

	static inline float3 Float3ToRadians(const float3 a)
	{
		return float3(ToRadians(a.x), ToRadians(a.y), ToRadians(a.z));
	}

	// FLOAT3
	//--------------------------------------------------------
	// FLOAT4

	static inline float4 operator+(const float4 a_lhs, const float4 a_rhs)
	{
		return float4(AddFloat4(a_lhs.vec, a_rhs.vec));
	}

	static inline float4 operator-(const float4 a_lhs, const float4 a_rhs)
	{
		return float4(SubFloat4(a_lhs.vec, a_rhs.vec));
	}

	static inline float4 operator*(const float4 a_lhs, const float a_float)
	{
		return float4(MulFloat4(a_lhs.vec, a_float));
	}

	static inline float4 operator*(const float4 a_lhs, const float3 a_rhs)
	{
		return float4(a_lhs.x * a_rhs.x, a_lhs.y * a_rhs.y, a_lhs.z * a_rhs.z, a_lhs.w);
	}

	static inline float4 operator*(const float4 a_lhs, const float4 a_rhs)
	{
		return float4(MulFloat4(a_lhs.vec, a_rhs.vec));
	}

	static inline float4 operator*(const float4x4 a_mat, const float4 a_vec)
	{
		float4 vec;
		vec.x = a_vec.x * a_mat.r0.x + a_vec.y * a_mat.r1.x + a_vec.z * a_mat.r2.x + a_vec.w * a_mat.r3.x;
		vec.y = a_vec.x * a_mat.r0.y + a_vec.y * a_mat.r1.y + a_vec.z * a_mat.r2.y + a_vec.w * a_mat.r3.y;
		vec.z = a_vec.x * a_mat.r0.z + a_vec.y * a_mat.r1.z + a_vec.z * a_mat.r2.z + a_vec.w * a_mat.r3.z;
		vec.w = a_vec.x * a_mat.r0.w + a_vec.y * a_mat.r1.w + a_vec.z * a_mat.r2.w + a_vec.w * a_mat.r3.w;
		return vec;
	}

    static inline float4 operator*(const float4 a_vec, const float4x4 a_mat)
    {
        float4 vec;
        vec.x = a_vec.x * a_mat.r0.x + a_vec.y * a_mat.r0.y + a_vec.z * a_mat.r0.z + a_vec.w * a_mat.r0.w;
        vec.y = a_vec.x * a_mat.r1.x + a_vec.y * a_mat.r1.y + a_vec.z * a_mat.r1.z + a_vec.w * a_mat.r1.w;
        vec.z = a_vec.x * a_mat.r2.x + a_vec.y * a_mat.r2.y + a_vec.z * a_mat.r2.z + a_vec.w * a_mat.r2.w;
        vec.w = a_vec.x * a_mat.r3.x + a_vec.y * a_mat.r3.y + a_vec.z * a_mat.r3.z + a_vec.w * a_mat.r3.w;
        return vec;
    }

	static inline float4 operator/(const float4 a_lhs, const float4 a_rhs)
	{
		return float4(DivFloat4(a_lhs.vec, a_rhs.vec));
	}

	static inline float Float4Dot(const float4 a, const float4 b)
	{
		const float4 mod = MulFloat4(a.vec, b.vec);
		return mod.x + mod.y + mod.z + mod.w;
	}

	static inline float Float4LengthSq(const float4 a)
	{
		return Float4Dot(a, a);
	}

	static inline float Float4Length(const float4 a)
	{
		return sqrtf(Float4LengthSq(a));
	}

	static inline float4 Float4Normalize(const float4 a)
	{
		const float length = Float4Length(a);
		const float rcp_length = 1.0f / length;
		return a * rcp_length;
	}

	static inline float4 Float4Min(const float4 a_lhs, const float4 a_rhs)
	{
		return MinFloat4(a_lhs.vec, a_rhs.vec);
	}

	static inline float4 Float4Max(const float4 a_lhs, const float4 a_rhs)
	{
		return MaxFloat4(a_lhs.vec, a_rhs.vec);
	}

	static inline float4 Float4Clamp(const float4 a_value, const float4 a_min, const float4 a_max)
	{
		return MinFloat4(MaxFloat4(a_value.vec, a_min.vec), a_max.vec);
	}

	// FLOAT4
	//--------------------------------------------------------
	// QUAT

	static inline Quat operator*(const Quat a_lhs, const float a_float)
	{
		return Quat{ MulFloat4(a_lhs.vec, a_float) };
	}

	static inline Quat operator*(const Quat a_lhs, const Quat a_rhs)
	{
		return Quat{ MulFloat4(a_lhs.vec, a_rhs.vec) };
	}

	static inline Quat IdentityQuat()
	{
		return Quat{ LoadFloat4(0, 0, 0, 1) };
	}

	static inline Quat QuatFromAxisAngle(const float3 axis, const float angle)
	{
		//const float3 normAxis = Float3Normalize(axis);

		const float s = sinf(0.5f * angle);

		Quat quat;
		quat.xyz = axis * s;
		quat.w = cosf(angle * 0.5f);
		return quat;
	}

	static inline Quat QuatRotateQuat(const Quat a, const Quat b)
	{
		return Quat{ MulFloat4(a.vec, b.vec) };
	}

	// QUAT
    //--------------------------------------------------------
    // FLOAT3x3

	static inline float3x3 operator*(const float3x3& a_lhs, const float3x3& a_rhs)
	{
		float3x3 mat;
		mat.r0 = a_lhs.r0 * a_rhs.r0.x + a_lhs.r1 * a_rhs.r0.y + a_lhs.r2 * a_rhs.r0.z;
		mat.r1 = a_lhs.r0 * a_rhs.r1.x + a_lhs.r1 * a_rhs.r1.y + a_lhs.r2 * a_rhs.r1.z;
		mat.r2 = a_lhs.r0 * a_rhs.r2.x + a_lhs.r1 * a_rhs.r2.y + a_lhs.r2 * a_rhs.r2.z;
		return mat;
	}

	static inline float3x3 Float3x3FromFloats(
		const float m00, const float m01, const float m02,
		const float m10, const float m11, const float m12,
		const float m20, const float m21, const float m22)
	{
		float3x3 mat;
		mat.e[0][0] = m00;
		mat.e[0][1] = m01;
		mat.e[0][2] = m02;

		mat.e[1][0] = m10;
		mat.e[1][1] = m11;
		mat.e[1][2] = m12;

		mat.e[2][0] = m20;
		mat.e[2][1] = m21;
		mat.e[2][2] = m22;
		return mat;
	}

	static inline float3x3 Float3x3Identity()
	{
		float3x3 mat{};
		mat.e[0][0] = 1.f;
		mat.e[1][1] = 1.f;
		mat.e[2][2] = 1.f;
		return mat;
	}
	
	static inline float3x3 Float3x3FromRotation(const float3 a_rotation)
	{
		float3x3 mat = Float3x3Identity();

		mat.e[0][0] = cosf(a_rotation.y) * cosf(a_rotation.z);
		mat.e[0][1] = cosf(a_rotation.y) * sinf(a_rotation.z);
		mat.e[0][2] = -sinf(a_rotation.y);

		mat.e[1][0] = sinf(a_rotation.x) * sinf(a_rotation.y) * cosf(a_rotation.z) - cosf(a_rotation.x) * sinf(a_rotation.z);
		mat.e[1][1] = sinf(a_rotation.x) * sinf(a_rotation.y) * sinf(a_rotation.z) + cosf(a_rotation.x) * cosf(a_rotation.z);
		mat.e[1][2] = sinf(a_rotation.x) * cosf(a_rotation.y);

		mat.e[2][0] = cosf(a_rotation.x) * sinf(a_rotation.y) * cosf(a_rotation.z) + sinf(a_rotation.x) * sinf(a_rotation.z);
		mat.e[2][1] = cosf(a_rotation.x) * sinf(a_rotation.y) * sinf(a_rotation.z) - sinf(a_rotation.x) * cosf(a_rotation.z);
		mat.e[2][2] = cosf(a_rotation.x) * cosf(a_rotation.y);

		return mat;
	}

	static inline float3x3 Float3x3FromRotationY(const float a_rotation_y)
	{
		float3x3 mat = Float3x3Identity();

		mat.e[0][0] = cosf(a_rotation_y);
		mat.e[0][1] = 0.f;
		mat.e[0][2] = -sinf(a_rotation_y);

		mat.e[1][0] = 0.f;
		mat.e[1][1] = 1.f;
		mat.e[1][2] = 0.f;

		mat.e[2][0] = sinf(a_rotation_y);
		mat.e[2][1] = 0.f;
		mat.e[2][2] = cosf(a_rotation_y);

		return mat;
	}

	static inline float3x3 Float3x3FromQuat(const Quat q)
	{
		float3x3 rot_mat = Float3x3Identity();
		const float qxx = q.x * q.x;
		const float qyy = q.y * q.y;
		const float qzz = q.z * q.z;

		const float qxz = q.x * q.z;
		const float qxy = q.x * q.y;
		const float qyz = q.y * q.z;

		const float qwx = q.w * q.x;
		const float qwy = q.w * q.y;
		const float qwz = q.w * q.z;

		rot_mat.e[0][0] = 1.f - 2.f * (qyy + qzz);
		rot_mat.e[0][1] = 2.f * (qxy + qwz);
		rot_mat.e[0][2] = 2.f * (qxz - qwy);

		rot_mat.e[1][0] = 2.f * (qxy - qwz);
		rot_mat.e[1][1] = 1.f - 2.f * (qxx + qzz);
		rot_mat.e[1][2] = 2.f * (qyz + qwx);

		rot_mat.e[2][0] = 2.f * (qxz + qwy);
		rot_mat.e[2][1] = 2.f * (qyz - qwx);
		rot_mat.e[2][2] = 1.f - 2.f * (qxx + qyy);
		return rot_mat;
	}

	// FLOAT3x3
	//--------------------------------------------------------
	// FLOAT4x4

	static inline float4x4 Float4x4FromFloats(
		float m00, float m01, float m02, float m03,
		float m10, float m11, float m12, float m13,
		float m20, float m21, float m22, float m23,
		float m30, float m31, float m32, float m33)
	{
		float4x4 mat;
		mat.vec[0] = LoadFloat4(m00, m01, m02, m03);
		mat.vec[1] = LoadFloat4(m10, m11, m12, m13);
		mat.vec[2] = LoadFloat4(m20, m21, m22, m23);
		mat.vec[3] = LoadFloat4(m30, m31, m32, m33);
		return mat;
	}

	static inline float4x4 Float4x4FromFloat4s(const float4 r0, const float4 r1, const float4 r2, const float4 r3)
	{
		float4x4 mat;
		mat.vec[0] = LoadFloat4(r0.x, r0.y, r0.z, r0.w);
		mat.vec[1] = LoadFloat4(r1.x, r1.y, r1.z, r1.w);
		mat.vec[2] = LoadFloat4(r2.x, r2.y, r2.z, r2.w);
		mat.vec[3] = LoadFloat4(r3.x, r3.y, r3.z, r3.w);
		return mat;
	}

    static inline float4x4 Float4x4Transpose(const float4x4& a_mat)
    {
        return Float4x4FromFloats(
            a_mat.e[0][0], a_mat.e[1][0], a_mat.e[2][0], a_mat.e[3][0],
            a_mat.e[0][1], a_mat.e[1][1], a_mat.e[2][1], a_mat.e[3][1],
            a_mat.e[0][2], a_mat.e[1][2], a_mat.e[2][2], a_mat.e[3][2],
            a_mat.e[0][3], a_mat.e[1][3], a_mat.e[2][3], a_mat.e[3][3]);
    }

	static inline float4x4 operator*(const float4x4& a_lhs, const float4x4& a_rhs)
	{
		float4x4 mat;
		mat.r0 = a_lhs.r0 * a_rhs.r0.x + a_lhs.r1 * a_rhs.r0.y + a_lhs.r2 * a_rhs.r0.z + a_lhs.r3 * a_rhs.r0.w;
		mat.r1 = a_lhs.r0 * a_rhs.r1.x + a_lhs.r1 * a_rhs.r1.y + a_lhs.r2 * a_rhs.r1.z + a_lhs.r3 * a_rhs.r1.w;
		mat.r2 = a_lhs.r0 * a_rhs.r2.x + a_lhs.r1 * a_rhs.r2.y + a_lhs.r2 * a_rhs.r2.z + a_lhs.r3 * a_rhs.r2.w;
		mat.r3 = a_lhs.r0 * a_rhs.r3.x + a_lhs.r1 * a_rhs.r3.y + a_lhs.r2 * a_rhs.r3.z + a_lhs.r3 * a_rhs.r3.w;
		return mat;
	}

	static inline float4x4 operator*(const float4x4& a_lhs, const float3x3& a_rhs)
	{
		return Float4x4FromFloats(
			a_lhs.r0.x * a_rhs.r0.x, a_lhs.r0.y * a_rhs.r0.y, a_lhs.r0.z * a_rhs.r0.z, a_lhs.r0.w,
			a_lhs.r1.x * a_rhs.r1.x, a_lhs.r1.y * a_rhs.r1.y, a_lhs.r1.z * a_rhs.r1.z, a_lhs.r1.w,
			a_lhs.r2.x * a_rhs.r2.x, a_lhs.r2.y * a_rhs.r2.y, a_lhs.r2.z * a_rhs.r2.z, a_lhs.r2.w,
			a_lhs.r3.x, a_lhs.r3.y, a_lhs.r3.z, a_lhs.r3.w);
	}

	static inline float4x4 Float4x4Identity()
	{
		float4x4 mat;
		mat.vec[0] = LoadFloat4(1, 0, 0, 0);
		mat.vec[1] = LoadFloat4(0, 1, 0, 0);
		mat.vec[2] = LoadFloat4(0, 0, 1, 0);
		mat.vec[3] = LoadFloat4(0, 0, 0, 1);
		return mat;
	}

	static inline float4x4 Float4x4FromTranslation(const float3 translation)
	{
		float4x4 result = Float4x4Identity();
		result.r3.x = translation.x;
		result.r3.y = translation.y;
		result.r3.z = translation.z;
		return result;
	}

	static inline float4x4 Float4x4FromQuat(const Quat q)
	{
		float4x4 rot_mat = Float4x4Identity();
		const float qxx = q.x * q.x;
		const float qyy = q.y * q.y;
		const float qzz = q.z * q.z;

		const float qxz = q.x * q.z;
		const float qxy = q.x * q.y;
		const float qyz = q.y * q.z;

		const float qwx = q.w * q.x;
		const float qwy = q.w * q.y;
		const float qwz = q.w * q.z;

		rot_mat.e[0][0] = 1.f - 2.f * (qyy + qzz);
		rot_mat.e[0][1] = 2.f * (qxy + qwz);
		rot_mat.e[0][2] = 2.f * (qxz - qwy);

		rot_mat.e[1][0] = 2.f * (qxy - qwz);
		rot_mat.e[1][1] = 1.f - 2.f * (qxx + qzz);
		rot_mat.e[1][2] = 2.f * (qyz + qwx);

		rot_mat.e[2][0] = 2.f * (qxz + qwy);
		rot_mat.e[2][1] = 2.f * (qyz - qwx);
		rot_mat.e[2][2] = 1.f - 2.f * (qxx + qyy);
		return rot_mat;
	}

	static inline float4x4 Float4x4FromRotation(const float3 a_rotation)
	{
		float4x4 mat = Float4x4Identity();

		mat.e[0][0] = cosf(a_rotation.y) * cosf(a_rotation.z);
		mat.e[0][1] = cosf(a_rotation.y) * sinf(a_rotation.z);
		mat.e[0][2] = -sinf(a_rotation.y);

		mat.e[1][0] = sinf(a_rotation.x) * sinf(a_rotation.y) * cosf(a_rotation.z) - cosf(a_rotation.x) * sinf(a_rotation.z);
		mat.e[1][1] = sinf(a_rotation.x) * sinf(a_rotation.y) * sinf(a_rotation.z) + cosf(a_rotation.x) * cosf(a_rotation.z);
		mat.e[1][2] = sinf(a_rotation.x) * cosf(a_rotation.y);

		mat.e[2][0] = cosf(a_rotation.x) * sinf(a_rotation.y) * cosf(a_rotation.z) + sinf(a_rotation.x) * sinf(a_rotation.z);
		mat.e[2][1] = cosf(a_rotation.x) * sinf(a_rotation.y) * sinf(a_rotation.z) - sinf(a_rotation.x) * cosf(a_rotation.z);
		mat.e[2][2] = cosf(a_rotation.x) * cosf(a_rotation.y);

		return mat;
	}

	static inline float4x4 Float4x4Scale(const float4x4& m, const float3 s)
	{
		float4x4 mat;
		mat.r0 = m.r0 * s.x;
		mat.r1 = m.r1 * s.y;
		mat.r2 = m.r2 * s.z;
		mat.r3 = m.r3;
		return mat;
	}

	static inline float4x4 Float4x4Perspective(const float a_fov, const float a_aspect, const float a_near, const float a_far)
	{
		const float tan_half_fov = tan(a_fov / 2.f);

		float4x4 mat{};
		mat.e[0][0] = 1.f / (a_aspect * tan_half_fov);
		mat.e[1][1] = 1.f / (tan_half_fov);
		mat.e[2][2] = -(a_far + a_near) / (a_far - a_near);
		mat.e[3][2] = -1.f;
		mat.e[2][3] = -(2.f * a_far * a_near) / (a_far - a_near);

		mat.e[1][1] *= -1;

		return mat;
	}

	static inline float4x4 Float4x4Ortographic(const float a_left, const float a_right, const float a_bottom, const float a_top, const float a_near, const float a_far)
	{
		float4x4 mat = Float4x4Identity();
		// scale
		mat.e[0][0] = 2 / (a_right - a_left);
		mat.e[1][1] = 2 / (a_top - a_bottom);
		mat.e[2][2] = 2 / (a_far - a_near);

		mat.e[0][3] = -(a_right + a_left) / (a_right - a_left);
		mat.e[1][3] = -(a_top + a_bottom) / (a_top - a_bottom);
		mat.e[2][3] = -(a_far + a_near) / (a_far - a_near);

		return mat;
	}

	static inline float4x4 Float4x4Lookat(const float3 a_eye, const float3 a_center, const float3 a_up)
	{
		const float3 forward = Float3Normalize(a_center - a_eye);
		const float3 right = Float3Normalize(Float3Cross(forward, a_up));
		const float3 up = Float3Normalize(Float3Cross(right, forward));

		float4x4 mat;
        mat.r0 = float4(right.x, right.y, right.z, -Float3Dot(right, a_eye));
        mat.r1 = float4(up.x, up.y, up.z, -Float3Dot(up, a_eye));
        mat.r2 = float4(-forward.x, -forward.y, -forward.z, Float3Dot(forward, a_eye));
        mat.r3 = float4(0.f, 0.f, 0.f, 1.f);

		return mat;
	}

    static inline void Float4x4ExtractView(const float4x4& a_view, float3& a_right, float3& a_up, float3& a_forward)
    {
        a_right = float3(a_view.r0.x, a_view.r0.y, a_view.r0.z);
        a_up = float3(a_view.r1.x, a_view.r1.y, a_view.r1.z);
        a_forward = float3(-a_view.r2.x, -a_view.r2.y, -a_view.r2.z);
    }

	static inline float4x4 Float4x4Inverse(const float4x4& m)
	{
		const float3 a = float3{ m.e[0][0], m.e[1][0], m.e[2][0] };
		const float3 b = float3{ m.e[0][1], m.e[1][1], m.e[2][1] };
		const float3 c = float3{ m.e[0][2], m.e[1][2], m.e[2][2] };
		const float3 d = float3{ m.e[0][3], m.e[1][3], m.e[2][3] };

		const float x = m.e[3][0];
		const float y = m.e[3][1];
		const float z = m.e[3][2];
		const float w = m.e[3][3];

		float3 s = Float3Cross(a, b);
		float3 t = Float3Cross(c, d);

		float3 u = (a * y) - (b * x);
		float3 v = (c * w) - (d * z);

		const float inv_det = 1.0f / (Float3Dot(s, v) + Float3Dot(t, u));
		s = (s * inv_det);
		t = (t * inv_det);
		u = (u * inv_det);
		v = (v * inv_det);

		const float3 r0 = (Float3Cross(b, v) + (t * y));
		const float3 r1 = (Float3Cross(v, a) - (t * x));
		const float3 r2 = (Float3Cross(d, u) + (s * w));
		const float3 r3 = (Float3Cross(u, c) - (s * z));

		return Float4x4FromFloats(
			r0.x, r0.y, r0.z, -Float3Dot(b, t),
			r1.x, r1.y, r1.z, Float3Dot(a, t),
			r2.x, r2.y, r2.z, -Float3Dot(d, s),
			r3.x, r3.y, r3.z, Float3Dot(c, s));
	}

	static inline float3 Float4x4ExtractTranslation(const float4x4& a_transform)
	{
		return float3(a_transform.r3.x, a_transform.r3.y, a_transform.r3.z);
	}

	static inline float3x3 Float3x3ExtractRotationFromFloat4x4(const float4x4& a_transform, const float3 a_scale)
	{
		return Float3x3FromFloats(
			a_transform.e[0][0] / a_scale.x, a_transform.e[0][1] / a_scale.x, a_transform.e[0][2] / a_scale.x,
			a_transform.e[1][0] / a_scale.y, a_transform.e[1][1] / a_scale.y, a_transform.e[1][2] / a_scale.y,
			a_transform.e[2][0] / a_scale.z, a_transform.e[2][1] / a_scale.z, a_transform.e[2][2] / a_scale.z);
	}

	static inline float3 Float4x4ExtractScale(const float4x4& a_transform)
	{
		return float3(
			Float3Length(float3(a_transform.r0.x, a_transform.r1.x, a_transform.r2.x)),
			Float3Length(float3(a_transform.r0.y, a_transform.r1.y, a_transform.r2.y)),
			Float3Length(float3(a_transform.r0.z, a_transform.r1.z, a_transform.r2.z)));
	}

	static inline float4x4 Float4x4ExtractRotationAsFloat4x4(const float4x4& a_transform, const float3 a_scale)
	{
		return Float4x4FromFloats(
			a_transform.e[0][0] / a_scale.x, a_transform.e[0][1] / a_scale.x, a_transform.e[0][2] / a_scale.x, 0,
			a_transform.e[0][1] / a_scale.y, a_transform.e[1][1] / a_scale.y, a_transform.e[1][2] / a_scale.y, 0,
			a_transform.e[0][2] / a_scale.z, a_transform.e[2][1] / a_scale.z, a_transform.e[2][2] / a_scale.z, 0,
			0, 0, 0, 1);
	}

	//thank you Mike Day from Insomniac games for this code, I have no clue how the math works.
	//https://d3cw3dd2w32x2b.cloudfront.net/wp-content/uploads/2015/01/matrix-to-quat.pdf //may not exist anymore, ah well.
	static inline Quat Float4x4ExtractRotationAsQuad(const float4x4& a_transform, const float3 a_scale)
	{
		const float4x4 rotation_mat = Float4x4ExtractRotationAsFloat4x4(a_transform, a_scale);

		float t = 0;
		Quat quat;

		//did I miss anything here? Maybe. 
		if (rotation_mat.e[2][2] < 0)
		{
			if (rotation_mat.e[0][0] > rotation_mat.e[1][1])
			{
				t = 1 + rotation_mat.e[0][0] - rotation_mat.e[1][1] - rotation_mat.e[2][2];
				quat = Quat(
					t,
					rotation_mat.e[0][1] + rotation_mat.e[1][0],
					rotation_mat.e[2][0] + rotation_mat.e[0][2],
					rotation_mat.e[1][2] - rotation_mat.e[2][1]);
			}
			else
			{
				t = 1 - rotation_mat.e[0][0] + rotation_mat.e[1][1] - rotation_mat.e[2][2];
				quat = Quat(
					rotation_mat.e[0][1] + rotation_mat.e[1][0],
					t,
					rotation_mat.e[1][2] + rotation_mat.e[2][1],
					rotation_mat.e[2][0] - rotation_mat.e[0][2]);
			}
		}
		else
		{
			if (rotation_mat.e[0][0] < -rotation_mat.e[1][1])
			{
				t = 1 - rotation_mat.e[0][0] - rotation_mat.e[1][1] + rotation_mat.e[2][2];
				quat = Quat(
					rotation_mat.e[2][0] - rotation_mat.e[0][2],
					rotation_mat.e[1][2] + rotation_mat.e[2][1],
					t,
					rotation_mat.e[0][1] - rotation_mat.e[1][0]);
			}
			else
			{
				t = 1 + rotation_mat.e[0][0] + rotation_mat.e[1][1] + rotation_mat.e[2][2];
				quat = Quat(
					rotation_mat.e[1][2] - rotation_mat.e[2][1],
					rotation_mat.e[2][0] - rotation_mat.e[0][2],
					rotation_mat.e[0][1] - rotation_mat.e[1][0],
					t);
			}
		}

		return quat * (0.5f / sqrtf(t));
	}

	static inline void Float4x4DecomposeTransform(const float4x4& a_transform, float3& a_translation, float3x3& a_rotation, float3& a_scale)
	{
		a_translation = Float4x4ExtractTranslation(a_transform);
		a_scale = Float4x4ExtractScale(a_transform);
		a_rotation = Float3x3ExtractRotationFromFloat4x4(a_transform, a_scale);
	}
}
