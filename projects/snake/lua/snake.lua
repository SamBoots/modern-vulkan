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

map_size = 20
map_size_min = 5

snake_size = 0
snake_pos = {}
snake_dir_x = 0
snake_dir_y = -1

move_timer = 0.5
update_time = 0

apple_pos = {}

game_state_current = 0
game_state_main_menu = 0
game_state_game = 3
game_state_dead = 2
game_state_pause = 1

function Init()
    return true
end

function PosHitTable2D(a_table, a_table_size, a_pos_x, a_pos_y)
    for i=1,a_table_size do
        local pos_x, pos_y = table.unpack(a_table[i])
        if a_pos_x == pos_x and a_pos_y == pos_y then
            return i
        end
	end
    return 0
end

function RandomPos2DEmptyTile()
    local pos_x = math.random(1, map_size)
    local pos_y = math.random(1, map_size)
    local occupied = false
    if PosHitTable2D(snake_pos, snake_size, pos_x, pos_y) == 0 and PosHitTable2D(apple_pos, #apple_pos, pos_x, pos_y) == 0 then
        return pos_x, pos_y
    end
    return RandomPos2DEmptyTile()
end

function NewGame()
    snake_size = 1
    snake_pos[1] = { math.floor(map_size / 2), math.floor(map_size / 2) }
    local apples = map_size / map_size_min
    apple_pos = {}
    for i=1,apples do
        pos_x, pos_y = RandomPos2DEmptyTile();
        apple_pos[i] = {pos_x, pos_y}
    end
end

function WrapAroundPos(new_pos_x, new_pos_y)
    local mod_pos_x = ((new_pos_x - 1) % map_size) + 1
    local mod_pos_y = ((new_pos_y - 1) % map_size) + 1
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
        local next_pos_x, next_pos_y = WrapAroundPos(cur_pos_x + move_value_x, cur_pos_y + move_value_y)
        
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

    new_pos_x, new_pos_y = WrapAroundPos(new_pos_x, new_pos_y)
    if PosHitTable2D(snake_pos, snake_size - 1, new_pos_x, new_pos_y) ~= 0 then
        game_state_current = game_state_dead
        return
    end

    for i=1,snake_size do
        snake_pos[snake_size - i + 2] = snake_pos[snake_size - i + 1]
	end
    snake_pos[1] = {new_pos_x, new_pos_y}

    local apple_index = PosHitTable2D(apple_pos, #apple_pos, new_pos_x, new_pos_y)
    if apple_index ~= 0 then
        snake_size = snake_size + 1
        local apple_pos_x, apple_pos_y = RandomPos2DEmptyTile()
        apple_pos[apple_index] = { apple_pos_x, apple_pos_y }
    end
end

function DrawPanelCenter(a_text, a_r, a_g, a_b, a_a, a_screen_x, a_screen_y)
    local message_panel_scale = 0.6
    local message_panel_offset = 1 - message_panel_scale

    pos_x = a_screen_x * message_panel_offset / 2
    pos_y = a_screen_y * message_panel_offset / 2

    UICreatePanel(pos_x, pos_y, a_screen_x * message_panel_scale, a_screen_y * message_panel_scale, a_r, a_g, a_b, a_a)
    UICreateText(pos_x, pos_y, 0.9, 0.9, 255, 0, 0, 255, 0, 500, a_text)
end

function MainMenuDraw()
    local screen_x, screen_y = GetScreenResolution()
    UICreatePanel(0, 0, screen_x, screen_y, 0, 0, 0, 255)
    message = "press: ENTER to start the game\n" .. "press: A or D to decrease or increase the amount of tiles per side: " .. map_size
    DrawPanelCenter(message, 0, 0, 255, 100, screen_x, screen_y)
end

function DrawTable2D(a_table, a_table_size, a_start_offset_x, a_start_offset_y, a_tile_size, a_pixel_offset, a_r, a_g, a_b, a_a)
    for i=1,a_table_size do
        local pos_x, pos_y = table.unpack(a_table[i])
        pos_x = a_start_offset_x + (pos_x - 1) * (a_tile_size + a_pixel_offset)
        pos_y = a_start_offset_y + (pos_y - 1) * (a_tile_size + a_pixel_offset)

	    UICreatePanel(pos_x, pos_y, a_tile_size, a_tile_size, a_r, a_g, a_b, a_a)
    end
end

function GameDraw()
    local screen_x, screen_y = GetScreenResolution()
    UICreatePanel(0, 0, screen_x, screen_y, 0, 0, 0, 255)
    local pixel_offset = 4
    local margin = 32
    local screen_min = math.min(screen_x, screen_y) - (margin * 2)
    local tile_size = (screen_min - (pixel_offset * (map_size - 1))) / map_size
    -- make sure there is a tile space so leave one side.

    local start_x = (screen_x - (map_size * tile_size + (map_size - 1) * pixel_offset)) / 2
    local start_y = (screen_y - (map_size * tile_size + (map_size - 1) * pixel_offset)) / 2

    for y=1,map_size do  
        for x=1,map_size do  
            local pos_x = start_x + (x - 1) * (tile_size + pixel_offset)
            local pos_y = start_y + (y - 1) * (tile_size + pixel_offset)

            UICreatePanel(pos_x, pos_y, tile_size, tile_size, 255, 255, 255, 255)
        end
    end

    DrawTable2D(snake_pos, snake_size, start_x, start_y, tile_size, pixel_offset, 0, 255, 0, 255)
    DrawTable2D(apple_pos, #apple_pos, start_x, start_y, tile_size, pixel_offset, 255, 0, 0, 255)

    local total_size_x_size = map_size * tile_size
    total_size_x_size = total_size_x_size / 3
    local message = "current score: " .. snake_size - 1
    UICreateText(total_size_x_size, 0, 0.9, 0.9, 255, 0, 0, 255, 0, 700, message)
end

function RestartDraw()
    GameDraw()
    local screen_x, screen_y = GetScreenResolution()
    local message = "score achieved: " .. snake_size - 1 .. "\npress: ENTER to go back to main menu"
    DrawPanelCenter(message, 0, 255, 0, 100, screen_x, screen_y)
end

function PauseDraw()
    GameDraw()
    local screen_x, screen_y = GetScreenResolution()
    DrawPanelCenter("paused\npress: ENTER to go back to main menu", 255, 0, 0, 100, screen_x, screen_y)
end

function MainMenuUpdate(a_selected)
    MainMenuDraw()

    if a_selected then
        if InputActionIsPressed(start_game) then
            NewGame()
            game_state_current = game_state_game
        end

        if InputActionIsPressed(increase_tiles) then
            map_size = map_size + 1
        elseif InputActionIsPressed(decrease_tiles) then
            map_size = math.max(map_size_min, map_size - 1)
        end
    end
end

function GameUpdate(a_delta_time, a_selected)
    GameDraw()

    if a_selected then
         if InputActionIsPressed(pause_game) then
            game_state_current = game_state_pause
        end
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
    end
end

function RestartUpdate(a_selected)
    RestartDraw()

    if a_selected then
        if InputActionIsPressed(start_game) then
            game_state_current = game_state_main_menu
        end
    end
end

function PauseUpdate(a_selected)
    PauseDraw()

    if a_selected then
        if InputActionIsPressed(start_game) then
            game_state_current = game_state_main_menu
        end
        if InputActionIsPressed(pause_game) then
            update_time = 0
            game_state_current = game_state_game
        end
    end
end

function Update(a_delta_time, a_selected)
    if game_state_current == game_state_main_menu then
        MainMenuUpdate(a_selected)
    elseif game_state_current == game_state_game then
        GameUpdate(a_delta_time, a_selected)
    elseif game_state_current == game_state_pause then
        PauseUpdate(a_selected)
    elseif game_state_current == game_state_dead then
        RestartUpdate(a_selected)
    end
    
    return true
end

function Destroy()
    
end
