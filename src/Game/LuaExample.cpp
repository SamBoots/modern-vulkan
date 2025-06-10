#include "LuaExample.hpp"
#include "BBjson.hpp"
#include "Program.h"
#include "HID.h"
#include "AssetLoader.hpp"

#include "Math/Math.inl"
#include "Math/Collision.inl"

#include "InputSystem.hpp"

using namespace BB;

bool LuaExample::Init(const uint2 a_game_viewport_size, const uint32_t a_back_buffer_count)
{
    m_memory = MemoryArenaCreate();
    m_scene_hierarchy.Init(m_memory, STANDARD_ECS_OBJ_COUNT, a_game_viewport_size, a_back_buffer_count, "lua example hierarchy");
    m_viewport.Init(a_game_viewport_size, int2(0, 0), "lua example viewport");

    m_context.Init(m_memory, gbSize);



    return true;
}
