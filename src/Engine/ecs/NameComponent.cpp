#include "NameComponent.hpp"

#include "MemoryArena.hpp"

using namespace BB;

void NameComponentPool::Init(struct MemoryArena& a_arena, const uint32_t a_transform_count)
{
	m_components.Init(a_arena, a_transform_count);
	m_components.resize(a_transform_count);
}

bool NameComponentPool::CreateComponent(const ECSEntity a_entity)
{
	if (EntityInvalid(a_entity))
		return false;

	new (&m_components[a_entity.index]) NameComponent("unnamed");
	return true;
}

bool NameComponentPool::CreateComponent(const ECSEntity a_entity, const NameComponent& a_component)
{
	if (EntityInvalid(a_entity))
		return false;

	new (&m_components[a_entity.index]) NameComponent(a_component);
	return true;
}

bool NameComponentPool::FreeComponent(const ECSEntity a_entity)
{
	if (EntityInvalid(a_entity))
		return false;

	return true;
}

NameComponent& NameComponentPool::GetComponent(const ECSEntity a_entity) const
{
	BB_ASSERT(!EntityInvalid(a_entity), "entity entry is not valid!");
	return m_components[a_entity.index];
}

bool NameComponentPool::EntityInvalid(const ECSEntity a_entity) const
{
	if (a_entity.index >= m_components.size())
		return true;
	return false;
}
