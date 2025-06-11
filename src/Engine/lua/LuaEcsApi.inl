#include "lualib.h"
#include "lauxlib.h"
#include "lua.hpp"

#include "ecs/EntityComponentSystem.hpp"

namespace BB
{
    namespace luaapi
    {
        static float luaGetFloat(lua_State* m_state, const int a_arg_index)
        {
            return static_cast<float>(lua_tonumber(m_state, a_arg_index));
        }

        static EntityComponentSystem* GetECS(lua_State* m_state)
        {
           return reinterpret_cast<EntityComponentSystem*>(lua_touserdata(m_state, lua_upvalueindex(1)));
        }

        static int ECSCreateEntity(lua_State* m_state)
        {
            const NameComponent name = lua_tostring(m_state, 1);
            const ECSEntity parent = ECSEntity(lua_tointeger(m_state, 2));
            const float pos_x = luaGetFloat(m_state, 3);
            const float pos_y = luaGetFloat(m_state, 4);
            const float pos_z = luaGetFloat(m_state, 5);

            EntityComponentSystem* ecs = GetECS(m_state);
            const ECSEntity entity = ecs->CreateEntity(name, parent, float3(pos_x, pos_y, pos_z));

            lua_pushinteger(m_state, entity.handle);
            return 1;
        }

        static int ECSGetPosition(lua_State* m_state)
        {
            const ECSEntity entity = ECSEntity(lua_tointeger(m_state, 1));
            EntityComponentSystem* ecs = GetECS(m_state);
            
            const float3 pos = ecs->GetPosition(entity);

            lua_pushnumber(m_state, pos.x);
            lua_pushnumber(m_state, pos.y);
            lua_pushnumber(m_state, pos.z);
            return 3;
        }

        static int ECSSetPosition(lua_State* m_state)
        {
            const ECSEntity entity = ECSEntity(lua_tointeger(m_state, 1));
            const float pos_x = luaGetFloat(m_state, 2);
            const float pos_y = luaGetFloat(m_state, 3);
            const float pos_z = luaGetFloat(m_state, 4);

            EntityComponentSystem* ecs = GetECS(m_state);
            ecs->SetPosition(entity, float3(pos_x, pos_y, pos_z));

            return 0;
        }

        static int ECSTranslate(lua_State* m_state)
        {
            const ECSEntity entity = ECSEntity(lua_tointeger(m_state, 1));
            const float pos_x = luaGetFloat(m_state, 2);
            const float pos_y = luaGetFloat(m_state, 3);
            const float pos_z = luaGetFloat(m_state, 4);

            EntityComponentSystem* ecs = GetECS(m_state);
            ecs->Translate(entity, float3(pos_x, pos_y, pos_z));

            return 0;
        }
    }
}
