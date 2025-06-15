#include "LuaExample.hpp"
#include "BBjson.hpp"
#include "Program.h"
#include "HID.h"
#include "AssetLoader.hpp"

#include "Math/Math.inl"
#include "Math/Collision.inl"

#include "InputSystem.hpp"

#include "lua.h"
#include "lauxlib.h"

#include "lua/LuaTypes.hpp"

using namespace BB;

constexpr const char* lua_scene = R"( 
    local FreeCamera = {}

    function FreeCamera.new(a_pos, a_forward, a_right, a_up, a_speed)
    local camera = 
        {
            m_pos = a_pos or float3(0),
            m_forward = a_forward or float3(0, 0, -1),
            m_right = a_right or float3(1, 0, 0),
            m_up = a_up or float3(0, 1, 0),
            m_speed = a_speed or 1,
            m_velocity_speed = 25.0,
            m_yaw = 90,
            m_pitch = 0,
            m_velocity = float3(0)
        }
        return camera;
    end

    function FreeCamera:Move(a_x, a_y, a_z)
        local velocity = float3(0)
        velocity = velocity + self.m_forward * a_z
        local cross = self.m_forward.Cross(self.m_up)
        velocity = velocity + cross.normalize() * a_x
        velocity = velocity + self.m_up * a_y
        self.m_velocity = self.m_velocity + velocity * self.m_speed;
    end

    function FreeCamera:Rotate(a_yaw, a_pitch)
        self.m_yaw = self.m_yaw + a_yaw
        self.m_pitch = self.m_pitch + a_pitch
        if self.m_pitch > 90.0 then
            self.m_pitch = 90.0
        elseif self.m_pitch < -90.0 then
            self.m_pitch = -90.0
        end
        
        local dir_x = math.cos(self.m_yaw) * math.cos(self.m_pitch)
        local dir_y = math.sin(self.m_pitch)
        local dir_z = math.sin(self.m_yaw) * math.cos(self.m_pitch)
        local direction = float3(dir_x, dir_y, dir_z)
        
        self.m_forward = direction.Normalize()
        local cross = self.m_up.Cross(self.m_forward)
        self.m_right = cross.Normalize()
    end
    
    function FreeCamera:Update(a_delta_time)
        local velocity = self.m_velocity * self.m_velocity_speed * a_delta_time;
        self.m_velocity = self.m_velocity - velocity;
        self.m_pos = self.m_pos + velocity;
    end

    camera = FreeCamera.new(float3(0, 0, 0), float3(1, 0, 0), float3(0, 0, -1), float3(0, 1, 0), 1)

    function GetCameraPos()
        return camera.m_pos
    end

    function GetCameraUp()
        return camera.m_up
    end

    function GetCameraForward()
        return camera.m_forward
    end

    function MoveCamera(a_x, a_y, a_z)
        camera:Move(a_x, a_y, a_z)
    end

    function RotateCamera(a_x, a_y)
        camera:Rotate(a_x, a_y)
    end

    function update(a_delta_time, selected)
        camera:Update(a_delta_time)
    end
)";

bool LuaExample::Init(const uint2 a_game_viewport_size, const uint32_t a_back_buffer_count)
{
    m_memory = MemoryArenaCreate();
    m_scene_hierarchy.Init(m_memory, STANDARD_ECS_OBJ_COUNT, a_game_viewport_size, a_back_buffer_count, "lua example hierarchy");
    m_viewport.Init(a_game_viewport_size, int2(0, 0), "lua example viewport");

    m_context.Init(m_memory, &m_scene_hierarchy.GetECS(), gbSize);

    int status = luaL_dostring(m_context.GetState(), lua_scene);
    BB_ASSERT(status == LUA_OK, lua_tostring(m_context.GetState(), -1));
    return true;
}

bool LuaExample::Update(const float a_delta_time, const bool a_selected)
{ 
    const LuaStackScope scope(m_context.GetState());
    lua_getglobal(m_context.GetState(), "update");
    lua_pushnumber(m_context.GetState(), static_cast<lua_Number>(a_delta_time));
    lua_pushboolean(m_context.GetState(), a_selected);
    lua_pcall(m_context.GetState(), 2, 0, 0);

    const float3 pos = GetCameraPos();

    lua_getglobal(m_context.GetState(), "GetCameraUp");
    lua_pcall(m_context.GetState(), 0, 1, 0);
    const float3 up = *lua_getfloat3(m_context.GetState(), -1);
    lua_getglobal(m_context.GetState(), "GetCameraForward");
    lua_pcall(m_context.GetState(), 0, 1, 0);
    const float3 forward = *lua_getfloat3(m_context.GetState(), -1);

    const float4x4 view = Float4x4Lookat(pos, pos + forward, up);
    m_scene_hierarchy.GetECS().GetRenderSystem().SetView(view, pos);

    return true;
}

float3 LuaExample::GetCameraPos() 
{
    const LuaStackScope scope(m_context.GetState());
    lua_getglobal(m_context.GetState(), "GetCameraPos");
    lua_pcall(m_context.GetState(), 0, 1, 0);
    return *lua_getfloat3(m_context.GetState(), -1);
}
