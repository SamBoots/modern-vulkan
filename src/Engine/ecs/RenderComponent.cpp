#include "RenderComponent.hpp"
#include "Math.inl"

#include "MemoryArena.hpp"

using namespace BB;

void RenderComponentPool::Init(struct MemoryArena& a_arena, const uint32_t a_render_mesh_count)
{
	m_components.Init(a_arena, a_render_mesh_count);
	m_components.resize(a_render_mesh_count);
}

bool RenderComponentPool::CreateComponent(const ECSEntity a_entity)
{
	if (EntityInvalid(a_entity))
		return false;

	m_components[a_entity.index] = {};
	return true;
}

bool RenderComponentPool::CreateComponent(const ECSEntity a_entity, const RenderComponent& a_component)
{
	if (EntityInvalid(a_entity))
		return false;

	m_components[a_entity.index] = a_component;
	return true;
}

bool RenderComponentPool::FreeComponent(const ECSEntity a_entity)
{
	if (EntityInvalid(a_entity))
		return false;

	return true;
}

RenderComponent& RenderComponentPool::GetComponent(const ECSEntity a_entity) const
{
	BB_ASSERT(!EntityInvalid(a_entity), "entity entry is not valid!");
	return m_components[a_entity.index];
}

bool RenderComponentPool::EntityInvalid(const ECSEntity a_entity) const
{
	if (a_entity.index >= m_components.size())
		return true;
	return false;
}
