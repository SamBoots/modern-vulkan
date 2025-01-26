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

		bool CreateEntity(ECSEntity& a_out_entity, const ECSEntity a_parent = ECSEntity());
		bool FreeEntity(const ECSEntity a_entity);
		bool EntitySetParent(const ECSEntity a_entity, const ECSEntity a_parent);

		// returns false if the signature is already set.
		bool RegisterSignature(const ECSEntity a_entity, const ECSSignatureIndex a_signature_index);
		// returns false if one of the signature is already set.
		bool RegisterSignatures(const ECSEntity a_entity, const ConstSlice<ECSSignatureIndex> a_signature_indices);
		// returns false if the signature was not set.
		bool UnregisterSignature(const ECSEntity a_entity, const ECSSignatureIndex a_signature_index);
		// returns false if one of the signature was not set.
		bool UnregisterSignatures(const ECSEntity a_entity, const ConstSlice<ECSSignatureIndex> a_signature_indices);

		ECSEntity GetParent(const ECSEntity a_entity) const;
		bool GetSignature(const ECSEntity a_entity, ECSSignature& a_out_signature) const;
		bool HasSignature(const ECSEntity a_entity, const ECSSignatureIndex a_signature_index) const;
		bool HasSignature(const ECSSignature a_signature, const ECSSignatureIndex a_signature_index) const;

		bool ValidateEntity(const ECSEntity a_entity) const;
	private:
		bool EntityWithinBounds(const ECSEntity a_entity) const;
		uint32_t m_entity_count;
		SPSCQueue<ECSEntity> m_entity_queue;
		
		struct ECSSignatureSentinel
		{
			ECSEntity parent;
			ECSSignature signature;
			uint32_t sentinel; // same as ECSEntity.extra_index
		};
		StaticArray<ECSSignatureSentinel> m_entities;
	};
}
