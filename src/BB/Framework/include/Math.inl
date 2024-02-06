#pragma once
#include "Common.h"
#include <cmath>

namespace BB
{
	constexpr float PI_F = 3.14159265358979323846f;
	constexpr double PI_D = 3.14159265358979323846;

	static inline float ToRadians(const float degrees)
	{
		return degrees * 0.01745329252f;
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

	static inline float3 operator/(const float3 a_lhs, const float3 a_rhs)
	{
		return float3{ a_lhs.x / a_rhs.x, a_lhs.y / a_rhs.y, a_lhs.z / a_rhs.z };
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

	// FLOAT3
	//--------------------------------------------------------
	// FLOAT4

	static inline float4 operator+(const float4 a_lhs, const float4 a_rhs)
	{
		return float4{ AddFloat4(a_lhs.vec, a_rhs.vec) };
	}

	static inline float4 operator-(const float4 a_lhs, const float4 a_rhs)
	{
		return float4{ SubFloat4(a_lhs.vec, a_rhs.vec) };
	}

	static inline float4 operator*(const float4 a_lhs, const float a_float)
	{
		return float4{ MulFloat4(a_lhs.vec, a_float) };
	}

	static inline float4 operator*(const float4 a_lhs, const float4 a_rhs)
	{
		return float4{ MulFloat4(a_lhs.vec, a_rhs.vec) };
	}

	static inline float4 operator/(const float4 a_lhs, const float4 a_rhs)
	{
		return float4{ DivFloat4(a_lhs.vec, a_rhs.vec) };
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

	static inline float4x4 operator*(const float4x4& a_lhs, const float4x4 a_rhs)
	{
		float4x4 mat;
		mat.r0 = a_lhs.r0 * a_rhs.r0.x + a_lhs.r1 * a_rhs.r0.y + a_lhs.r2 * a_rhs.r0.z + a_lhs.r3 * a_rhs.r0.w;
		mat.r1 = a_lhs.r0 * a_rhs.r1.x + a_lhs.r1 * a_rhs.r1.y + a_lhs.r2 * a_rhs.r1.z + a_lhs.r3 * a_rhs.r1.w;
		mat.r2 = a_lhs.r0 * a_rhs.r2.x + a_lhs.r1 * a_rhs.r2.y + a_lhs.r2 * a_rhs.r2.z + a_lhs.r3 * a_rhs.r2.w;
		mat.r3 = a_lhs.r0 * a_rhs.r3.x + a_lhs.r1 * a_rhs.r3.y + a_lhs.r2 * a_rhs.r3.z + a_lhs.r3 * a_rhs.r3.w;
		return mat;
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
		result.e[3][0] = translation.x;
		result.e[3][1] = translation.y;
		result.e[3][2] = translation.z;
		return result;
	}

	static inline float4x4 Float4x4FromQuat(const Quat q)
	{
		float4x4 rotMat = Float4x4Identity();
		const float qxx = q.x * q.x;
		const float qyy = q.y * q.y;
		const float qzz = q.z * q.z;

		const float qxz = q.x * q.z;
		const float qxy = q.x * q.y;
		const float qyz = q.y * q.z;

		const float qwx = q.w * q.x;
		const float qwy = q.w * q.y;
		const float qwz = q.w * q.z;

		rotMat.e[0][0] = 1.f - 2.f * (qyy + qzz);
		rotMat.e[0][1] = 2.f * (qxy + qwz);
		rotMat.e[0][2] = 2.f * (qxz - qwy);

		rotMat.e[1][0] = 2.f * (qxy - qwz);
		rotMat.e[1][1] = 1.f - 2.f * (qxx + qzz);
		rotMat.e[1][2] = 2.f * (qyz + qwx);

		rotMat.e[2][0] = 2.f * (qxz + qwy);
		rotMat.e[2][1] = 2.f * (qyz - qwx);
		rotMat.e[2][2] = 1.f - 2.f * (qxx + qyy);
		return rotMat;
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

	static inline float4x4 Float4x4Perspective(const float fov, const float aspect, const float nearField, const float farField)
	{
		const float tanHalfFov = tan(fov / 2.f);

		float4x4 mat{};
		mat.e[0][0] = 1.f / (aspect * tanHalfFov);
		mat.e[1][1] = 1.f / (tanHalfFov);
		mat.e[2][2] = -(farField + nearField) / (farField - nearField);
		mat.e[2][3] = -1.f;
		mat.e[3][2] = -(2.f * farField * nearField) / (farField - nearField);

		return mat;
	}

	static inline float4x4 Float4x4Lookat(const float3 eye, const float3 center, const float3 up)
	{
		const float3 f = Float3Normalize(center - eye);
		const float3 s = Float3Normalize(Float3Cross(f, up));
		const float3 u = Float3Cross(s, f);

		float4x4 mat = Float4x4Identity();
		mat.e[0][0] = s.x;
		mat.e[1][0] = s.y;
		mat.e[2][0] = s.z;
		mat.e[0][1] = u.x;
		mat.e[1][1] = u.y;
		mat.e[2][1] = u.z;
		mat.e[0][2] = -f.x;
		mat.e[1][2] = -f.y;
		mat.e[2][2] = -f.z;
		mat.e[3][0] = -Float3Dot(s, eye);
		mat.e[3][1] = -Float3Dot(u, eye);
		mat.e[3][2] = Float3Dot(f, eye);
		return mat;
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
		return float3(a_transform.r0.w, a_transform.r1.w, a_transform.r2.w);
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
			a_transform.e[1][0] / a_scale.x, a_transform.e[1][1] / a_scale.x, a_transform.e[1][2] / a_scale.x, 0,
			a_transform.e[2][0] / a_scale.x, a_transform.e[2][1] / a_scale.x, a_transform.e[2][2] / a_scale.x, 0,
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

	static inline void Float4x4DecomposeTransform(const float4x4& a_transform, float3& a_translation, Quat& a_rotation, float3& a_scale)
	{
		a_translation = Float4x4ExtractTranslation(a_transform);
		a_scale = Float4x4ExtractScale(a_transform);
		a_rotation = Float4x4ExtractRotationAsQuad(a_transform, a_scale);
	}
}
