#pragma once
#include "Enginefwd.hpp"
#include "Storage/Queue.hpp"
#include "Storage/Array.h"

namespace BB
{


	class EntityMap
	{
	public:
		bool Init(MemoryArena& a_arena, const uint32_t a_max_entities);

		bool CreateEntity(ECSEntity& a_out_entity);
		bool FreeEntity(const ECSEntity a_entitiy);

		// retursn false if the signature is already set.
		bool RegisterSignature(const ECSEntity a_entity, const ECSSignatureIndex a_signature_index);
		// returns false if the signature was not set.
		bool UnregisterSignature(const ECSEntity a_entity, const ECSSignatureIndex a_signature_index);

		ECSSignature GetSignature(const ECSEntity a_entity) const;

	private:
		uint32_t m_entity_max;
		uint32_t m_entity_count;
		SPSCQueue<ECSEntity> m_entity_queue;
		// maybe make it safe with a sentinel value
		StaticArray<ECSSignature> m_entities;
	}
}
