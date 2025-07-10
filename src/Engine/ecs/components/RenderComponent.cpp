#include "RenderComponent.hpp"
#include "Math/Math.inl"

#include "MaterialSystem.hpp"

using namespace BB;

void RenderComponentPool::Init(struct MemoryArena& a_arena, const uint32_t a_render_mesh_count, const uint32_t a_entity_count)
{
    m_components.Init(a_arena, a_render_mesh_count);
    m_components.resize(a_render_mesh_count);

    m_sparse_set.Init(a_arena, a_entity_count, a_render_mesh_count);
}

bool RenderComponentPool::CreateComponent(const ECSEntity a_entity)
{
    if (m_components.capacity() == m_sparse_set.Size())
        return false;

    const uint32_t component_index = m_sparse_set.Insert(a_entity);
    if (component_index == SPARSE_SET_INVALID)
        return false;

    m_components[component_index] = {};
    return true;
}

bool RenderComponentPool::CreateComponent(const ECSEntity a_entity, const RenderComponent& a_component)
{
    if (m_components.capacity() == m_sparse_set.Size())
        return false;

    const uint32_t component_index = m_sparse_set.Insert(a_entity);
    if (component_index == SPARSE_SET_INVALID)
        return false;

    m_components[component_index] = a_component;
    return true;
}

bool RenderComponentPool::FreeComponent(const ECSEntity a_entity)
{
    if (m_sparse_set.Size() == 0)
        return false;
    const RenderComponent& component = GetComponent(a_entity);
    if (component.material.IsValid())
        Material::FreeMaterialInstance(component.material);
    return m_sparse_set.Erase(a_entity);
}

RenderComponent& RenderComponentPool::GetComponent(const ECSEntity a_entity) const
{
	const uint32_t dense_index = m_sparse_set.Find(a_entity.index);
	BB_ASSERT(dense_index != SPARSE_SET_INVALID, "invalid sparse set index returned");
	return m_components[dense_index];
}

ConstSlice<ECSEntity> RenderComponentPool::GetEntityComponents() const
{
	return m_sparse_set.GetDense();
}
