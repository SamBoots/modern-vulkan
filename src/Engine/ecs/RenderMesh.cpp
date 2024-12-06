#include "RenderMesh.hpp"
#include "Math.inl"

#include "MemoryArena.hpp"

using namespace BB;

void RenderMeshPool::Init(struct MemoryArena& a_arena, const uint32_t a_render_mesh_count)
{
	m_components.Init(a_arena, a_render_mesh_count);
	m_components.resize(a_render_mesh_count);
}

bool RenderMeshPool::CreateComponent(const ECSEntity a_entity)
{
	if (EntityInvalid(a_entity))
		return false;

	m_components[a_entity.index] = {};
	return true;
}

bool RenderMeshPool::CreateComponent(const ECSEntity a_entity, const RenderMesh& a_component)
{
	if (EntityInvalid(a_entity))
		return false;

	m_components[a_entity.index] = a_component;
	return true;
}

bool RenderMeshPool::FreeComponent(const ECSEntity a_entity)
{
	if (EntityInvalid(a_entity))
		return false;

	return true;
}

RenderMesh& RenderMeshPool::GetComponent(const ECSEntity a_entity)
{
	BB_ASSERT(!EntityInvalid(a_entity), "entity entry is not valid!");
	return m_components[a_entity.index];
}

bool RenderMeshPool::EntityInvalid(const ECSEntity a_entity) const
{
	if (a_entity.index >= m_components.size())
		return true;
	return false;
}
