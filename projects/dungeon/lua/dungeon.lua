local camera_module = require "camera"
local Camera = camera_module.Camera
local FreeCamera = camera_module.FreeCamera

local map_module = require "map"
local DungeonMap = map_module.DungeonMap

local Player = {}
Player.__index = Player

function Player.new(a_pos, a_forward, a_lerp_speed)
    local player = 
    {
        camera = Camera.new(a_pos, a_forward, float3(1, 0, 0), float3(0, 1, 0)),
        pos_src = a_pos,
        pos_dst = a_pos,
        pos_lerp = 0.0,

        forward_src = a_forward,
        forward_dst = a_forward,
        forward_lerp = 0.0,
        lerp_speed = a_lerp_speed
    }
    setmetatable(player, Player)
    return player
end

function Player:Move(a_move)
    self.pos_src = self.camera.pos
    self.pos_dst = self.pos_dst + a_move
    self.pos_lerp = 0.0
    return self.pos_dst
end

function Player:Rotate(a_rot)
    self.forward_src = self.camera.forward
    self.forward_dst = float3Rotate(a_rot, self.forward_dst);
    self.forward_lerp = 0.0
    return self.forward_dst
end

function Player:SetPos(a_pos)
    self.camera.pos = a_pos
end

function Player:GetPos()
    return self.camera.pos
end

function Player:SetLerpSpeed(a_lerp_speed)
    self.lerp_speed = a_lerp_speed
end

function Player:Update(a_delta_time)
    local lerp_speed = self.lerp_speed * a_delta_time

    self.pos_lerp = math.min(self.pos_lerp + lerp_speed, 1.0)
    self.camera.pos = self.pos_src + (self.pos_dst - self.pos_src) * self.pos_lerp

    self.forward_lerp = math.min(self.forward_lerp + lerp_speed, 1.0)
    self.camera.forward = self.forward_src + (self.forward_dst - self.forward_src) * self.forward_lerp
end

function Player:IsMoving()
    return self.pos_lerp < 1.0 or self.forward_lerp < 1.0
end

player = nil
free_cam = FreeCamera.new(Camera.new(float3(0, 0.5, -0.5), float3(0, 0, -1), float3(1, 0, 0), float3(0, 1, 0)), 1)
use_freecam = false

function GetCameraPos()
    if use_freecam then
        return free_cam.camera.pos
    else
        return player.camera.pos
    end
end

function GetCameraUp()
    if use_freecam then
        return free_cam.camera.up
    else
        return player.camera.up
    end
end

function GetCameraForward()
    if use_freecam then
        return free_cam.camera.forward
    else
        return player.camera.forward
    end
end

map = nil

function Init()
    map = DungeonMapViaFiles(40, 40, {"rooms/map1.txt"})
    player = Player.new(float3(map.spawn_x, 0.5, map.spawn_y), float3(0, 0, 1), 5)
    return true
end

function FreeCamMove(a_delta_time)
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

    free_cam:Update(a_delta_time)
end

function DoPlayerMove()
    local move_value_x, move_value_y = InputActionGetFloat2(player_move)
    local player_move = float3(move_value_x, 0, move_value_y)
    if move_value_x ~= 0 or move_value_y ~= 0 then
        player:Move(player_move)
    end

    if InputActionIsPressed(player_turn_left) then
        player:Rotate(float3(0,  math.rad(90), 0))
    end
    if InputActionIsPressed(player_turn_right) then
        player:Rotate(float3(0, math.rad(-90), 0))
    end
end

function SelectedUpdate(a_delta_time)
    if InputActionIsPressed(toggle_freecam) then
        use_freecam = not use_freecam
        free_cam.camera.pos = player.camera.pos
    end

    if use_freecam then
        FreeCamMove(a_delta_time)
        return
    end

    if not player:IsMoving() then
        DoPlayerMove()
    end
end

function Update(a_delta_time, selected)
    if selected then
        SelectedUpdate(a_delta_time)
    end

    player:Update(a_delta_time)

    return true
end

function Destroy()
    return ECSDestroyEntity(map.entity)
end
