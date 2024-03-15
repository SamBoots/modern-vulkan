#pragma once
#include "Common.h"

namespace BB
{
	struct MemoryArena;
	namespace Editor
	{
		void Init(MemoryArena& a_arena, const uint2 window_extent);
		void Update();
	}
}