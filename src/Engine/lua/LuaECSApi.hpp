#pragma once

struct lua_State;

namespace BB
{
    namespace luaapi
    {
        int ECSCreateEntity(lua_State* m_state);
        int ECSGetPosition(lua_State* m_state);
        int ECSSetPosition(lua_State* m_state);
        int ECSTranslate(lua_State* m_state);

        int InputActionIsPressed(lua_State* m_state);
        int InputActionIsHeld(lua_State* m_state);
        int InputActionIsReleased(lua_State* m_state);
        int InputActionGetFloat(lua_State* m_state);
        int InputActionGetFloat2(lua_State* m_state);
    }
}
