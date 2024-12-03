#pragma once
#include "Enginefwd.hpp"
#include "Storage/Array.h"

namespace BB
{
	template <typename T, typename Component>
	concept is_ecs_component_map = requires(T v, Component& a_out_component, const ECSEntity a_entity)
	{
		// return false if entity already exists
		{ v.CreateComponent(a_entity) } -> std::same_as<bool>;
		// return false if entity does not exist here
		{ v.FreeComponent(a_entity) } -> std::same_as<bool>;
		// return false if entity does not hold this component
		{ v.GetComponent(a_entity, a_out_component) } -> std::same_as<bool>;
	};

	template<typename T>
	class EcsComponentMap
	{
	public:
		bool CreateComponent(const ESCEntity a_entity);
		bool FreeComponent(const ESCEntity a_entity);
		bool GetComponent(const ESCEntity a_entity, T& a_out_component);

	private:
		uint32_t signature;
		StaticArray<uint32_t> m_component_indices;
		StaticArray<T> m_components;
	};
}
