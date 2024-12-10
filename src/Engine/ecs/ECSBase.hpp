#pragma once
#include "Enginefwd.hpp"
#include "Storage/Array.h"

namespace BB
{
	constexpr ECSSignatureIndex TRANSFORM_ECS_SIGNATURE = ECSSignatureIndex(0);
	constexpr ECSSignatureIndex RENDER_ECS_SIGNATURE = ECSSignatureIndex(1);
	constexpr ECSSignatureIndex LIGHT_ECS_SIGNATURE = ECSSignatureIndex(2);

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
}
