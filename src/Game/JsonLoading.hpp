#pragma once
#include "Enginefwd.hpp"

namespace BB
{
    InputChannelHandle CreateInputChannelByJson(MemoryArena& a_arena, const StringView a_app_name, const PathString& a_json_path);

}
