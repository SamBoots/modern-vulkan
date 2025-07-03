local camera_module = require "camera"
local Camera = camera_module.Camera
local FreeCamera = camera_module.FreeCamera

local map_module = require "map"
local DungeonMap = map_module.DungeonMap
local dungeon_dictionary = map_module.dungeon_dictionary

local Player = {}
Player.__index = Player

local PLAYER_UP = float3(0, 1, 0)

function Player.new(a_pos, a_forward, a_right, a_lerp_speed)
    local player = 
    {
        pos = a_pos,
        pos_src = a_pos,
        pos_dst = a_pos,
        pos_lerp = 0.0,

        right = a_right,

        forward = a_forward,
        forward_src = a_forward,
        forward_dst = a_forward,
        forward_lerp = 0.0,
        lerp_speed = a_lerp_speed
    }
    setmetatable(player, Player)
    return player
end

function Player:Move(a_right, a_forward)
    local right_cross = float3Cross(self.forward_dst, PLAYER_UP);
    local right = float3Normalize(right_cross) * a_right
    local forward = self.forward_dst * a_forward
    local new_pos = self.pos_dst + forward + right

    if map:TileWalkable(new_pos.x + 1, new_pos.z) then
        self.pos_src = self.pos
        self.pos_dst = new_pos

        self.pos_lerp = 0.0
    end

    return self.pos_dst
end

function Player:Rotate(a_rot)
    self.forward_src = self.forward
    self.forward_dst = float3Rotate(a_rot, self.forward_dst);
    self.forward_lerp = 0.0
    return self.forward_dst
end

function Player:SetPos(a_x, a_y)
    
    self.pos.x = a_x
    self.pos.z = a_y
end

function Player:GetPos()
    return self.pos.x, self.pos.z
end

function Player:SetLerpSpeed(a_lerp_speed)
    self.lerp_speed = a_lerp_speed
end

function Player:Update(a_delta_time)
    local lerp_speed = self.lerp_speed * a_delta_time

    self.pos_lerp = math.min(self.pos_lerp + lerp_speed, 1.0)
    self.pos = self.pos_src + (self.pos_dst - self.pos_src) * self.pos_lerp

    self.forward_lerp = math.min(self.forward_lerp + lerp_speed, 1.0)
    self.forward = self.forward_src + (self.forward_dst - self.forward_src) * self.forward_lerp
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
        return player.pos
    end
end

function GetCameraUp()
    if use_freecam then
        return free_cam.camera.up
    else
        return PLAYER_UP
    end
end

function GetCameraForward()
    if use_freecam then
        return free_cam.camera.forward
    else
        return player.forward
    end
end

map = nil

function Init()
    map = DungeonMapViaFiles(40, 40, {"rooms/map1.txt"})
    player = Player.new(float3(map.spawn_x, 0.5, map.spawn_y), float3(0, 0, 1), float3(1, 0, 0), 5)
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
    if move_value_x ~= 0 or move_value_y ~= 0 then
        player:Move(move_value_x, move_value_y)
    end

    if InputActionIsPressed(player_turn_left) then
        player:Rotate(float3(0,  math.rad(90), 0))
    end
    if InputActionIsPressed(player_turn_right) then
        player:Rotate(float3(0, math.rad(-90), 0))
    end
end

function SelectedUpdate(a_delta_time)
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
