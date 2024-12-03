#include "EntityMap.hpp"

using namespace BB;

bool EntityMap::Init(MemoryArena& a_arena, const uint32_t a_max_entities)
{
	if (m_entity_max != 0)
		return false;

	m_entity_max = a_max_entities;
	m_entity_count = 0;
	m_entity_queue.Init(a_arena, a_max_entities);
	m_entities.Init(a_arena, a_max_entities);

	for (uint32_t i = 0; i < a_max_entities; i++)
	{
		ECSEntity entity{ i };
		m_entity_queue.EnQueue(entity);
	}
	return true;
}

bool EntityMap::CreateEntity(ECSEntity& a_out_entity)
{
	if (m_entity_count > m_entity_max)
		return false;
	++m_entity_count;
	a_out_entity = m_entity_queue.DeQueue();
	return true;
}

bool EntityMap::FreeEntity(const ECSEntity a_entitiy)
{
	if (m_entity_count == 0)
		return false;
	--m_entity_count;

	return true;
}

// retursn false if the signature is already set.
bool EntityMap::RegisterSignature(const ECSEntity a_entity, const ECSSignatureIndex a_signature_index)
{
	if (m_entities[a_entity.handle][a_signature_index.handle] == true)
		return false;

	m_entities[a_entity.handle][a_signature_index.handle] = true;
	return true;
}

// returns false if the signature was not set.
bool EntityMap::UnregisterSignature(const ECSEntity a_entity, const ECSSignatureIndex a_signature_index)
{
	if (m_entities[a_entity.handle][a_signature_index.handle] == false)
	{
		return false;
	}
	m_entities[a_entity.handle][a_signature_index.handle] = false;
	return true;
}

ECSSignature EntityMap::GetSignature(const ECSEntity a_entity) const
{
	return m_entities[a_entity.handle];
}