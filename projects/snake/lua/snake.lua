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
tile_base_pos = 30
map = {}

function Init()
    map_size_x = 20
    map_size_y = 20
    tile_size = 20
    tile_offset = 2;

    map[map_size_x * map_size_y] = nil
    for i=1,map_size_x * map_size_y do
        map[i] = 0
    end

    return true
end

function SelectedUpdate(a_delta_time)


    return true;
end

function Update(a_delta_time, selected)

    for y=1,map_size_y do  
        for x=1,map_size_x do  
            index = y * map_size_x + x
            pos_x = x * tile_size + x * tile_offset + tile_base_pos
            pos_y = y * tile_size + y * tile_offset + tile_base_pos

            UICreatePanel(pos_x, pos_y, tile_size, tile_size, 255, 255, 255, 255);
        end
    end
    if selected then
        return SelectedUpdate(a_delta_time)
    end
    
    return true
end

function Destroy()
    
end
