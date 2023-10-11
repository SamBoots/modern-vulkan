#pragma once
#include "BBMemory.h"
#include "Common.h"

namespace BB
{
	class Transform
	{
	public:
		Transform(const float3 a_Position);
		Transform(const float3 a_Position, const float3 a_Axis, const float a_Radians);
		Transform(const float3 a_Position, const float3 a_Axis, const float a_Radians, const float3 a_Scale);

		void Translate(const float3 a_Translation);
		void Rotate(const float3 a_Axis, const float a_Radians);

		void SetPosition(const float3 a_Position);
		void SetRotation(const float3 a_Axis, const float a_Radians);
		void SetScale(const float3 a_Scale);

		const float4x4 CreateMatrix();

		//44 bytes class
		float3 m_Pos; //12
		Quat m_Rot; //28
		float3 m_Scale; //40
	};

	using TransformHandle = FrameworkHandle<struct TransformHandleTag>;
	/// <summary>
	/// A special pool that handles Transform allocations.
	/// It has a secondary pool of pointers that all point to memory regions in a CPU exposed GPU buffer.
	/// </summary>
	class TransformPool
	{
	public:
		/// <param name="a_system_allocator">The allocator that will allocate the pool.</param>
		/// <param name="a_MatrixSize">The amount of matrices you want to allocate. The a_GPUMemoryRegion needs to have enough space to hold them all.</param>
		TransformPool(Allocator a_system_allocator, const uint32_t a_MatrixSize);
		~TransformPool();

		TransformHandle CreateTransform(const float3 a_Position);
		TransformHandle CreateTransform(const float3 a_Position, const float3 a_Axis, const float a_Radians);
		TransformHandle CreateTransform(const float3 a_Position, const float3 a_Axis, const float a_Radians, const float3 a_Scale);
		void FreeTransform(const TransformHandle a_handle);
		Transform& GetTransform(const TransformHandle a_handle) const;

		const uint32_t PoolSize() const;
			
	private:
		struct TransformPool_inst* inst;
	};

}