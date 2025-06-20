#pragma once
#include "Enginefwd.hpp"

namespace BB
{
    InputChannelHandle CreateInputChannelByJson(MemoryArena& a_arena, const StringView a_app_name, const StringView a_json_path);
    void CreateEntitiesViaJson(class SceneHierarchy& a_hierarchy, const StringView a_json_path);

}
