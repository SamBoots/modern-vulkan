#pragma once
#include "Enginefwd.hpp"
#include "Storage/Array.h"

namespace BB
{
	constexpr ECSSignatureIndex TRANSFORM_ECS_SIGNATURE = ECSSignatureIndex(0);
	constexpr ECSSignatureIndex NAME_ECS_SIGNATURE = ECSSignatureIndex(1);
	constexpr ECSSignatureIndex RENDER_ECS_SIGNATURE = ECSSignatureIndex(2);
	constexpr ECSSignatureIndex LIGHT_ECS_SIGNATURE = ECSSignatureIndex(3);

	template <typename T, typename Component>
	concept is_ecs_component_map = requires(T v, const Component& a_component, const ECSEntity a_entity)
	{
		// return false if entity already exists
		{ v.CreateComponent(a_entity) } -> std::same_as<bool>;
		// return false if entity already exists
		{ v.CreateComponent(a_entity, a_component) } -> std::same_as<bool>;
		// return false if entity does not exist here
		{ v.FreeComponent(a_entity) } -> std::same_as<bool>;
		// return false if entity does not hold this component
		{ v.GetComponent(a_entity) } -> std::same_as<Component&>;

		{ v.GetSize() } -> std::same_as<uint32_t>;
		{ v.GetSignatureIndex() } -> std::same_as<ECSSignatureIndex>;
	};

	template <typename T, ECSSignatureIndex ECS_INDEX>
	class ECSComponentBase
	{
	public:
		void Init(struct MemoryArena& a_arena, const uint32_t a_transform_count)
		{
			m_components.Init(a_arena, a_transform_count);
			m_components.resize(a_transform_count);
		}

		bool CreateComponent(const ECSEntity a_entity)
		{
			if (EntityInvalid(a_entity))
				return false;

			new (&m_components[a_entity.index]) T();
			return true;
		}

		bool CreateComponent(const ECSEntity a_entity, const T& a_component)
		{
			if (EntityInvalid(a_entity))
				return false;

			new (&m_components[a_entity.index]) float3(a_component);
			return true;
		}
		bool FreeComponent(const ECSEntity a_entity)
		{
			if (EntityInvalid(a_entity))
				return false;

			return true;
		}
		T& GetComponent(const ECSEntity a_entity) const
		{
			BB_ASSERT(!EntityInvalid(a_entity), "entity entry is not valid!");
			return m_components[a_entity.index];
		}

		inline ECSSignatureIndex GetSignatureIndex() const
		{
			return ECS_INDEX;
		}
		inline uint32_t GetSize() const
		{
			return m_size;
		}

	private:
		bool EntityInvalid(const ECSEntity a_entity) const
		{
			if (a_entity.index >= m_components.size())
				return true;
			return false;
		}


		// components equal to entities.
		uint32_t m_size;
		StaticArray<T> m_components;
	};
}
