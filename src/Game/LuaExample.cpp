#include "LuaExample.hpp"
#include "BBjson.hpp"
#include "Program.h"
#include "HID.h"
#include "AssetLoader.hpp"

#include "Math/Math.inl"
#include "Math/Collision.inl"

#include "InputSystem.hpp"

using namespace BB;

constexpr const char* free_camera = R"(
    local FreeCamera = {}

    function FreeCamera.new(a_pos, a_forward, a_right, a_up, a_speed)
    local camera = 
        {
            m_pos = a_pos or float3(0),
            m_forward = a_forward or float3(0, 0, -1)
            m_right = a_right or float3(1, 0, 0)
            m_up = a_up or float3(0, 1, 0),
            m_speed = a_speed or 1,
            m_velocity_speed = 25.f
            m_yaw = 90,
            m_pitch = 0,
            m_velocity = float3(0)
        }
        return camera;
    end

    function FreeCamera:Move(a_x, a_y, a_z)
        velocity = float3(0)
        velocity = velocity + m_forward * a_z
        local cross = m_forward.Cross(m_up)
        velocity = velocity + cross.normalize() * a_x
        velocity = velocity + m_up * a_y
        m_velocity = m_velocity + velocity * speed;
    end

    function FreeCamera:Rotate(a_yaw, a_pitch)
        m_yaw = m_yaw + a_yaw
        m_pitch = m_pitch + a_pitch
        if m_pitch > 90 then
            m_pitch = 90
        else if m_pitch < -90.f
            m_pitch = -90
        
        dir_x = math.cos(m_yaw) * cosf(m_pitch)
        dir_y = math.sin(m_pitch)
        dir_z = math.sin(m_yaw) * cosf(m_pitch)
        local direction = float3(dir_x, dir_y, dir_z)
        
        m_forward = direction.Normalize()
        local cross = m_up.Cross(m_forward)
        m_right = cross.Normalize()
    end
    
    function FreeCamera:Update(a_delta_time)
        local velocity = m_velocity * m_velocity_speed * a_delta_time;
        m_velocity = m_velocity - velocity;
        m_pos = m_pos + velocity;
    end

)";

constexpr const char* lua_scene = R"( 

    function update(a_delta_time, selected)
        
    end
)";

bool LuaExample::Init(const uint2 a_game_viewport_size, const uint32_t a_back_buffer_count)
{
    m_memory = MemoryArenaCreate();
    m_scene_hierarchy.Init(m_memory, STANDARD_ECS_OBJ_COUNT, a_game_viewport_size, a_back_buffer_count, "lua example hierarchy");
    m_viewport.Init(a_game_viewport_size, int2(0, 0), "lua example viewport");

    m_context.Init(m_memory, &m_scene_hierarchy.GetECS(), gbSize);

    return true;
}
