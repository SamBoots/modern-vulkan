#include "LightComponent.hpp"
#include "Math.inl"

#include "MemoryArena.hpp"

using namespace BB;

void LightComponentPool::Init(struct MemoryArena& a_arena, const uint32_t a_light_count, const uint32_t a_entity_count)
{
	m_components.Init(a_arena, a_light_count);
	m_components.resize(a_light_count);

	m_sparse_set.Init(a_arena, a_entity_count, a_light_count);
}

bool LightComponentPool::CreateComponent(const ECSEntity a_entity)
{
	if (m_components.capacity() == m_sparse_set.Size())
		return false;

	const uint32_t component_index = m_sparse_set.Insert(a_entity);
	if (component_index == SPARSE_SET_INVALID)
		return false;

	m_components[component_index] = {};
	return true;
}

bool LightComponentPool::CreateComponent(const ECSEntity a_entity, const LightComponent& a_component)
{
	if (m_components.capacity() == m_sparse_set.Size())
		return false;

	const uint32_t component_index = m_sparse_set.Insert(a_entity);
	if (component_index == SPARSE_SET_INVALID)
		return false;

	m_components[component_index] = a_component;
	return true;
}

bool LightComponentPool::FreeComponent(const ECSEntity a_entity)
{
	if (m_sparse_set.Size() == 0)
		return false;

	return m_sparse_set.Erase(a_entity);
}

LightComponent& LightComponentPool::GetComponent(const ECSEntity a_entity) const
{
	const ECSEntity index = m_sparse_set.Find(a_entity.index);
	BB_ASSERT(a_entity != INVALID_ECS_OBJ, "invalid sparse set index returned");
	return m_components[a_entity.index];
}

ConstSlice<ECSEntity> LightComponentPool::GetEntityComponents() const
{
	return m_sparse_set.GetDense();
}
