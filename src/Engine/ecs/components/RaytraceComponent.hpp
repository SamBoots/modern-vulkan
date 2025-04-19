#pragma once
#include "ecs/ECSBase.hpp"
#include "Rendererfwd.hpp"

namespace BB
{
	struct RaytraceComponent
	{
        RAccelerationStruct acceleration_structure;
		GPUAddress acceleration_struct_address;

		uint64_t acceleration_buffer_offset;
        uint32_t build_size;
        uint32_t scratch_size;
        uint32_t scratch_update;
        bool needs_build;
        bool needs_rebuild;
	};

	class RaytraceComponentPool
	{
	public:
		void Init(struct MemoryArena& a_arena, const uint32_t a_component_count, const uint32_t a_entity_count);

		bool CreateComponent(const ECSEntity a_entity);
		bool CreateComponent(const ECSEntity a_entity, const RaytraceComponent& a_component);
		bool FreeComponent(const ECSEntity a_entity);
		RaytraceComponent& GetComponent(const ECSEntity a_entity) const;
		
		ConstSlice<ECSEntity> GetEntityComponents() const;

		inline ECSSignatureIndex GetSignatureIndex() const
		{
			return RAYTRACE_ECS_SIGNATURE;
		}
		inline uint32_t GetSize() const
		{
			return m_sparse_set.Size();
		}

	private:
		EntitySparseSet m_sparse_set;
		StaticArray<RaytraceComponent> m_components;
	};
	static_assert(is_ecs_component_map<RaytraceComponentPool, RaytraceComponent>);
}
