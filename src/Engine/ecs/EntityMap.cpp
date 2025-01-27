#include "EntityMap.hpp"

using namespace BB;

bool EntityMap::Init(MemoryArena& a_arena, const uint32_t a_max_entities)
{
	if (m_entities.capacity() != 0)
		return false;

	m_entity_count = 0;
	m_entity_queue.Init(a_arena, a_max_entities);
	m_entities.Init(a_arena, a_max_entities);
	m_entities.resize(a_max_entities);

	for (uint32_t i = 0; i < a_max_entities; i++)
	{
		ECSEntity entity;
		entity.index = i;
		entity.extra_index = 1;
		m_entity_queue.EnQueue(entity);
		m_entities[i].sentinel = 1;
	}
	return true;
}

bool EntityMap::CreateEntity(ECSEntity& a_out_entity)
{
	if (m_entity_count > m_entities.capacity())
		return false;
	++m_entity_count;
	a_out_entity = m_entity_queue.DeQueue();
	return true;
}

bool EntityMap::FreeEntity(const ECSEntity a_entity)
{
	if (m_entity_count == 0)
		return false;

	--m_entity_count;

	// increment sentinel value
	ECSEntity queue_entity;
	queue_entity.index = a_entity.index;
	queue_entity.extra_index = ++m_entities[a_entity.index].sentinel;
	m_entity_queue.EnQueue(queue_entity);

	return true;
}

bool EntityMap::RegisterSignature(const ECSEntity a_entity, const ECSSignatureIndex a_signature_index)
{
	if (!ValidateEntity(a_entity))
		return false;
	if (m_entities[a_entity.index].signature[a_signature_index.handle] == true)
		return false;

	m_entities[a_entity.index].signature[a_signature_index.handle] = true;
	return true;
}

bool EntityMap::RegisterSignatures(const ECSEntity a_entity, const ConstSlice<ECSSignatureIndex> a_signature_indices)
{
	if (!ValidateEntity(a_entity))
		return false;
	bool no_overlap_registrations = true;
	for (size_t i = 0; i < a_signature_indices.size(); i++)
	{
		if (m_entities[a_entity.index].signature[a_signature_indices[i].handle] == true)
			no_overlap_registrations = false;
		m_entities[a_entity.index].signature[a_signature_indices[i].handle] = true;
	}
	return no_overlap_registrations;
}

bool EntityMap::UnregisterSignature(const ECSEntity a_entity, const ECSSignatureIndex a_signature_index)
{
	if (!ValidateEntity(a_entity))
		return false;
	if (m_entities[a_entity.index].signature[a_signature_index.handle] == false)
	{
		return false;
	}
	m_entities[a_entity.index].signature[a_signature_index.handle] = false;
	return true;
}

bool EntityMap::UnregisterSignatures(const ECSEntity a_entity, const ConstSlice<ECSSignatureIndex> a_signature_indices)
{
	if (!ValidateEntity(a_entity))
		return false;
	bool no_overlap_registrations = true;
	for (size_t i = 0; i < a_signature_indices.size(); i++)
	{
		if (m_entities[a_entity.index].signature[a_signature_indices[i].handle] == false)
			no_overlap_registrations = false;
		m_entities[a_entity.index].signature[a_signature_indices[i].handle] = false;
	}
	return no_overlap_registrations;
}

bool EntityMap::GetSignature(const ECSEntity a_entity, ECSSignature& a_out_signature) const
{
	if (!ValidateEntity(a_entity))
		return false;
	a_out_signature = m_entities[a_entity.index].signature;
	return true;
}

bool EntityMap::HasSignature(const ECSEntity a_entity, const ECSSignatureIndex a_signature_index) const
{
	if (!ValidateEntity(a_entity))
		return false;

	return m_entities[a_entity.index].signature[a_signature_index.handle];
}

bool EntityMap::HasSignature(const ECSSignature a_signature, const ECSSignatureIndex a_signature_index) const
{
	return a_signature[a_signature_index.handle];
}

bool EntityMap::ValidateEntity(const ECSEntity a_entity) const
{
	if (!EntityWithinBounds(a_entity))
		return false;
	if (m_entities[a_entity.index].sentinel != a_entity.extra_index)
		return false;

	return true;
}

bool EntityMap::EntityWithinBounds(const ECSEntity a_entity) const
{
	if (a_entity.index >= m_entities.capacity())
		return false;
	return true;
}
