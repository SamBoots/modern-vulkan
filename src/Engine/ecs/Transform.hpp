#pragma once
#include "BBMemory.h"
#include "ECSBase.hpp"

namespace BB
{
	class Transform
	{
	public:
		Transform(const float3 a_position);
		Transform(const float3 a_position, const float3 a_axis, const float a_radians);
		Transform(const float3 a_position, const Quat a_rotation, const float3 a_scale);
		Transform(const float3 a_position, const float3 a_axis, const float a_radians, const float3 a_scale);

		void Translate(const float3 a_translation);
		void Rotate(const float3 a_axis, const float a_radians);

		void SetPosition(const float3 a_position);
		void SetRotation(const float3 a_axis, const float a_radians);
		void SetScale(const float3 a_scale);

		const float4x4 CreateMatrix();

		//44 bytes class
		float3 m_pos; //12
		Quat m_rot; //28
		float3 m_scale; //40
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
		void Init(struct MemoryArena& a_arena, const uint32_t a_transform_count);

		bool CreateComponent(const ECSEntity a_entity);
		bool CreateComponent(const ECSEntity a_entity, const Transform& a_component);
		bool FreeComponent(const ECSEntity a_entity);
		bool GetComponent(const ECSEntity a_entity, Transform& a_out_component);

		inline ECSSignatureIndex GetSignatureIndex() const
		{
			return TRANSFORM_ECS_SIGNATURE;
		}

		TransformHandle CreateTransform(const float3 a_position);
		TransformHandle CreateTransform(const float3 a_position, const float3 a_axis, const float a_radians);
		TransformHandle CreateTransform(const float3 a_position, const Quat a_rotation, const float3 a_scale);
		TransformHandle CreateTransform(const float3 a_position, const float3 a_axis, const float a_radians, const float3 a_scale);
		void FreeTransform(const TransformHandle a_handle);
		Transform& GetTransform(const TransformHandle a_handle) const;
		float4x4 GetTransformMatrix(const TransformHandle a_handle) const;

		uint32_t PoolSize() const;
			
	private:
		uint32_t m_transform_count;
		uint32_t m_next_free_transform;

		StaticArray<uint32_t> m_component_indices;
		StaticArray<Transform> m_components;
	};
	static_assert(is_ecs_component_map<TransformPool, Transform>);
}
