#include "LuaTest.hpp"
#include "LuaTypes.hpp"
#include "LuaEngine.hpp"
#include "lualib.h"
#include "lauxlib.h"
#include "Logger.h"

#include "Math/Math.inl"

using namespace BB;

static bool SimpleFunctionsTest(lua_State* a_state)
{
    const char* funcs = R"(
        function mul_numbers(base, multiplier)
            local result = base * multiplier
            return result
        end

        function float3_add_mul(a_a, a_b)
            local p = a_a + a_b;
            p = p * 2;
            return p;
        end)";

    if (luaL_dostring(a_state, funcs) != LUA_OK)
        return false;

    {
        LuaStackScope scope(a_state);
        lua_getglobal(a_state, "mul_numbers");

        lua_pushnumber(a_state, 50.0);
        lua_pushnumber(a_state, 1.5);

        if (lua_pcall(a_state, 2, 1, 0) != LUA_OK)
            return false;

        float damage = lua_tonumber(a_state, -1);
        if (damage != 75.0f)
            return false;
    }

    {
        LuaStackScope scope(a_state);
        lua_getglobal(a_state, "float3_add_mul");

        const float3 a = float3(1.f, 2.f, 3.f);
        const float3 b = float3(10.f, 20.f, 30.f);
        lua_pushfloat3(a_state, a);
        lua_pushfloat3(a_state, b);

        if (lua_pcall(a_state, 2, 1, 0) != LUA_OK)
            return false;

        const float3 value = *lua_getfloat3(a_state, -1);
        const float3 cppvalue = (a + b) * 2;
        if (value != cppvalue)
            return false;
    }
    return true;
}

bool BB::lua_RunBBTypeTest()
{
    MemoryArena arena = MemoryArenaCreate();
    LuaContext context;
    context.Init(arena, gbSize);

    lua_State* state = context.State();

    if (!SimpleFunctionsTest(state))
    {
        BB_WARNING(false, "LUA: SimpleFunctionTest failed", WarningType::HIGH);
        return false;
    }


    return true;
}
