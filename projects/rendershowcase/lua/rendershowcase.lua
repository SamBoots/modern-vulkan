local camera_module = require "camera"
local Camera = camera_module.Camera
local FreeCamera = camera_module.FreeCamera
require "bbmath"

free_cam = FreeCamera.new(Camera.new(float3(0.8, 0.5, 0), float3(0, 0, -1), float3(1, 0, 0), float3(0, 1, 0)), 1)

right = nil
up = nil
forward = nil

function GetCameraPos()
    return free_cam.camera.pos
end

function GetCameraUp()
    return up
end

function GetCameraForward()
    return forward
end

root_entity = nil

function Init()
    root_entity = CreateEntityFromJson("scene.json")
    return true
end

function SelectedUpdate(a_delta_time)
    local move_value_x, move_value_y = InputActionGetFloat2(camera_move)
    local player_move = float3(move_value_x, 0, move_value_y) * a_delta_time
    local wheel_move = InputActionGetFloat(move_speed_slider)
            
    free_cam:AddSpeed(wheel_move)
    free_cam:Move(player_move)
    local enable_rot = InputActionIsHeld(enable_rotate)
    if enable_rot then
        local look_x, look_y = InputActionGetFloat2(look_around);
        free_cam:Rotate(look_x * a_delta_time, look_y * a_delta_time)
    end
end

function Update(a_delta_time, selected)
    if selected then
        SelectedUpdate(a_delta_time)
    end
    local focus_pos = ECSGetPosition(root_entity)
    free_cam.camera.pos = float3Rotate(float3(0, 0.75 * a_delta_time, 0), free_cam.camera.pos - focus_pos)
    right, up, forward = LookAt(focus_pos, free_cam.camera.pos, free_cam.camera.up)
    free_cam:Update(a_delta_time)
    return true
end

function Destroy()
    return ECSDestroyEntity(root_entity)
end
