#pragma once

struct lua_State;

namespace BB
{
    namespace luaapi
    {
        int ECSCreateEntity(lua_State* a_state);
        int ECSDestroyEntity(lua_State* a_state);
        int ECSGetPosition(lua_State* a_state);
        int ECSSetPosition(lua_State* a_state);
        int ECSTranslate(lua_State* a_state);

        int CreateEntityFromJson(lua_State* a_state);

        int InputActionIsPressed(lua_State* a_state);
        int InputActionIsHeld(lua_State* a_state);
        int InputActionIsReleased(lua_State* a_state);
        int InputActionGetFloat(lua_State* a_state);
        int InputActionGetFloat2(lua_State* a_state);
    }
}
