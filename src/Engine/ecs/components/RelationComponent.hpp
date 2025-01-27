#pragma once
#include "ecs/ECSBase.hpp"

namespace BB
{
	struct EntityRelation
	{
		ECSEntity parent;
		size_t child_count;
		ECSEntity first_child;
		ECSEntity next;
	};

	using RelationComponentPool = ECSComponentBase<EntityRelation, RELATION_ECS_SIGNATURE>;

}
