#include "LuaTypes.hpp"
#include "lualib.h"
#include "lauxlib.h"

#include "ecs/EntityComponentSystem.hpp"

namespace BB
{
    namespace luaapi
    {
        static EntityComponentSystem* GetECS(lua_State* m_state)
        {
           return reinterpret_cast<EntityComponentSystem*>(lua_touserdata(m_state, lua_upvalueindex(1)));
        }

        static int ECSCreateEntity(lua_State* m_state)
        {
            const NameComponent name = lua_tostring(m_state, 1);
            const ECSEntity parent = ECSEntity(lua_tointeger(m_state, 2));
            const float3 pos = *lua_getfloat3(m_state, 3);

            EntityComponentSystem* ecs = GetECS(m_state);
            const ECSEntity entity = ecs->CreateEntity(name, parent, float3(pos));

            lua_pushinteger(m_state, entity.handle);
            return 1;
        }

        static int ECSGetPosition(lua_State* m_state)
        {
            const ECSEntity entity = ECSEntity(lua_tointeger(m_state, 1));
            EntityComponentSystem* ecs = GetECS(m_state);
            
            const float3 pos = ecs->GetPosition(entity);

            lua_pushfloat3(m_state, pos);
            return 1;
        }

        static int ECSSetPosition(lua_State* m_state)
        {
            const ECSEntity entity = ECSEntity(lua_tointeger(m_state, 1));
            const float3 pos = *lua_getfloat3(m_state, 2);

            EntityComponentSystem* ecs = GetECS(m_state);
            ecs->SetPosition(entity, pos);

            return 0;
        }

        static int ECSTranslate(lua_State* m_state)
        {
            const ECSEntity entity = ECSEntity(lua_tointeger(m_state, 1));
            const float3 move = *lua_getfloat3(m_state, 2);

            EntityComponentSystem* ecs = GetECS(m_state);
            ecs->Translate(entity, move);

            return 0;
        }
    }
}
