local camera_module = require "camera"
local Camera = camera_module.Camera
local FreeCamera = camera_module.FreeCamera
require "bbmath"

function GetCameraPos()
    return float3(0, 0, 0)
end

function GetCameraUp()
    return float3(0, 1, 0)
end

function GetCameraForward()
    return float3(0, 0, 1)
end

root_entity = nil

map_size_x = 0
map_size_y = 0
tile_size = 0
tile_offset = 0;
tile_base_pos = 60

snake_size = 0
snake_pos = {}

move_timer = 0.5
update_time = 0

snake_dir_x = 0
snake_dir_y = -1

apple_pos_x = 0
apple_pos_y = 0

function Init()
    NewGame()
    return true
end

function NewGame()
    map_size_x = 20
    map_size_y = 20
    tile_size = 24
    tile_offset = 8;
    snake_size = 1
    snake_pos[1] = { math.floor(map_size_x / 2), math.floor(map_size_y / 2) }
    NewApple();
end

function PosHitSnake(pos_x, pos_y)
    for i=1,snake_size - 1 do
        local snake_pos_x, snake_pos_y = table.unpack(snake_pos[i])
        if snake_pos_x == pos_x and snake_pos_y == pos_y then
            return true
        end
	end
    return false
end

function NewApple()
    apple_pos_x = math.random(1, map_size_x)
    apple_pos_y = math.random(1, map_size_y)
    if PosHitSnake() then
        NewApple(apple_pos_x, apple_pos_y)
    end
end

function WrapAroundSnakePos(new_pos_x, new_pos_y)
    local mod_pos_x = ((new_pos_x - 1) % map_size_x) + 1
    local mod_pos_y = ((new_pos_y - 1) % map_size_y) + 1
    return mod_pos_x, mod_pos_y
end

function SetSnakeDirection()
    local move_value_x, move_value_y = InputActionGetFloat2(snake_move)
    move_value_y = -move_value_y
    if move_value_x ~= 0 or move_value_y ~= 0 then

        if move_value_x ~= 0 then
            move_value_y = 0
        end

        local cur_pos_x, cur_pos_y = table.unpack(snake_pos[1])
        local next_pos_x = cur_pos_x + move_value_x
        local next_pos_y = cur_pos_y + move_value_y

        if snake_size > 1 then
            local back_x, back_y = table.unpack(snake_pos[2])
            if back_x == next_pos_x and back_y == next_pos_y then
                return
            end
        end

        snake_dir_x = move_value_x
        snake_dir_y = move_value_y
    end
end

function MoveSnake(move_x, move_y)
    local prev_pos_x, prev_pos_y = table.unpack(snake_pos[1])
    local new_pos_x = prev_pos_x + move_x
    local new_pos_y = prev_pos_y + move_y

    new_pos_x, new_pos_y = WrapAroundSnakePos(new_pos_x, new_pos_y)
    if PosHitSnake(new_pos_x, new_pos_y) then
        NewGame()
        return
    end

    if new_pos_x == apple_pos_x and new_pos_y == apple_pos_y then
        NewApple()
        snake_size = snake_size + 1
    end

    for i=1,snake_size do
        snake_pos[snake_size - i + 2] = snake_pos[snake_size - i + 1]
	end
    snake_pos[1] = {new_pos_x, new_pos_y}
end

function SelectedUpdate(a_delta_time)
    if InputActionIsHeld(snake_speed_up) then
        update_time = update_time + a_delta_time * 4
    else
        update_time = update_time + a_delta_time
    end

    SetSnakeDirection()

    if (update_time >= move_timer) then
        update_time = 0
        MoveSnake(snake_dir_x, snake_dir_y)
    end

    return true;
end

function DrawMap()
    for y=1,map_size_y do  
        for x=1,map_size_x do  
            local pos_x = x * tile_size + x * tile_offset + tile_base_pos
            local pos_y = y * tile_size + y * tile_offset + tile_base_pos

            UICreatePanel(pos_x, pos_y, tile_size, tile_size, 255, 255, 255, 255)
        end
    end

    for i=1,snake_size do
        local pos_x, pos_y = table.unpack(snake_pos[i])
        pos_x = pos_x * tile_offset + pos_x * tile_size + tile_base_pos
        pos_y = pos_y * tile_offset + pos_y * tile_size + tile_base_pos

	    UICreatePanel(pos_x, pos_y, tile_size, tile_size, 0, 255, 0, 255)
    end

    local mod_apple_pos_x = apple_pos_x * tile_size + apple_pos_x * tile_offset + tile_base_pos
    local mod_apple_pos_y = apple_pos_y * tile_size + apple_pos_y * tile_offset + tile_base_pos
    UICreatePanel(mod_apple_pos_x, mod_apple_pos_y, tile_size, tile_size, 255, 0, 0, 255)

    local total_size_x_size = map_size_x * tile_size + map_size_x * tile_offset + tile_base_pos
    total_size_x_size = total_size_x_size / 3
    local message = "current score: " .. snake_size - 1
    UICreateText(total_size_x_size, 0, 2, 2, 255, 255, 255, 255, 0, 700, message)
end

function Update(a_delta_time, selected)
    DrawMap()
    if selected then
        return SelectedUpdate(a_delta_time)
    end
    
    return true
end

function Destroy()
    
end
