#pragma once
#include "ecs/ECSBase.hpp"
#include "Rendererfwd.hpp"

namespace BB
{
	struct RenderComponent
	{
		Mesh mesh;
		MasterMaterialHandle master_material;
		MaterialHandle material;
		MeshMetallic material_data;
		uint32_t index_start;
		uint32_t index_count;
	};

	class RenderComponentPool
	{
	public:
		void Init(struct MemoryArena& a_arena, const uint32_t a_render_mesh_count, const uint32_t a_entity_count);

		bool CreateComponent(const ECSEntity a_entity);
		bool CreateComponent(const ECSEntity a_entity, const RenderComponent& a_component);
		bool FreeComponent(const ECSEntity a_entity);
		RenderComponent& GetComponent(const ECSEntity a_entity) const;
		
		ConstSlice<ECSEntity> GetEntityComponents() const;

		inline ECSSignatureIndex GetSignatureIndex() const
		{
			return RENDER_ECS_SIGNATURE;
		}
		inline uint32_t GetSize() const
		{
			return m_sparse_set.Size();
		}

	private:
		EntitySparseSet m_sparse_set;
		StaticArray<RenderComponent> m_components;
	};
	static_assert(is_ecs_component_map<RenderComponentPool, RenderComponent>);
}
