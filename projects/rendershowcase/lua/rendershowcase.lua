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
    local zoom_value = InputActionGetFloat(camera_zoom) * a_delta_time
    free_cam:Move(float3(0, 0, zoom_value))
end

function Update(a_delta_time, selected)
    if selected then
        SelectedUpdate(a_delta_time)
    end
    local focus_pos = ECSGetPosition(root_entity)
    free_cam.camera.pos = float3Rotate(float3(0, 0.75 * a_delta_time, 0), free_cam.camera.pos - focus_pos)
    right, up, forward = free_cam:LookAt(focus_pos)
    free_cam:Update(a_delta_time)
    return true
end

function Destroy()
    return ECSDestroyEntity(root_entity)
end
