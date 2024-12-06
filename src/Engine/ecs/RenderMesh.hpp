#pragma once
#include "BBMemory.h"
#include "ECSBase.hpp"
#include "Rendererfwd.hpp"

namespace BB
{
	struct RenderMesh
	{
		Mesh mesh;
		MasterMaterialHandle master_material;
		MaterialHandle material;
		MeshMetallic material_data;
		uint32_t index_start;
		uint32_t index_count;
		bool material_dirty;
	};

	class RenderMeshPool
	{
	public:
		void Init(struct MemoryArena& a_arena, const uint32_t a_render_mesh_count);

		bool CreateComponent(const ECSEntity a_entity);
		bool CreateComponent(const ECSEntity a_entity, const RenderMesh& a_component);
		bool FreeComponent(const ECSEntity a_entity);
		RenderMesh& GetComponent(const ECSEntity a_entity);

		inline ECSSignatureIndex GetSignatureIndex() const
		{
			return RENDERMESH_ECS_SIGNATURE;
		}

	private:
		bool EntityInvalid(const ECSEntity a_entity) const;

		// components equal to entities.
		StaticArray<RenderMesh> m_components;
	};
	static_assert(is_ecs_component_map<RenderMeshPool, RenderMesh>);
}
