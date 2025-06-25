local DungeonRoom = {}
DungeonRoom.__index = DungeonRoom

function DungeonRoom.new(a_size_x, a_size_y, a_tiles)
    local room = 
    {
        size_x = a_size_x,
        size_y = a_size_y,
        tiles = a_tiles
    }
    setmetatable(room, DungeonRoom)
    return room
end

function DungeonRoomViaFile(a_map_name)
    tiles, size_x, size_y = CreateMapTilesFromFile(a_map_name)
    room = DungeonRoom.new(size_x, size_y, tiles)
    return room
end

local DungeonTile = {}
DungeonTile.__index = DungeonTile

function DungeonTile.new(a_walkable, a_is_wall)
    local tile = 
    {
        walkable = a_walkable,
        is_wall = a_is_wall
    }
    setmetatable(tile, DungeonTile)
    return tile
end

local DungeonMap = {}
DungeonMap.__index = DungeonMap

function GetIFromXY(a_x, a_y, a_max_x)
    return a_x + a_y * a_max_x
end

function DungeonMap.new(a_size_x, a_size_y, a_rooms)
    local map_tiles = {}
    local spawn_point_x = 0;
    local spawn_point_y = 0;
    map_tiles[a_size_x * a_size_y] = nil;
    for i = 1, a_size_x * a_size_y do
        map_tiles[i] = DungeonTile.new(false, false)
    end

    for i = 1, #a_rooms do
	    local room = a_rooms[i]
        local max_x = a_size_x - room.size_x
        local max_y = a_size_y - room.size_y

        local start_x = math.random(1, max_x)
        local start_y = math.random(1, max_y)

        local end_x = start_x + room.size_x
        local end_y = start_y + room.size_y

        local room_x = 0
        local room_y = 0

        for y= start_y, end_y do
            for x = start_x, end_x do
                local index = GetIFromXY(x, y, a_size_x)
                local room_index = GetIFromXY(room_x + 1, room_y, room.size_x)

                local tile_value = room.tiles[room_index]
                if tile_value == 35 then
                    map_tiles[index] = DungeonTile.new(false, true)
                elseif tile_value == 46 then
                    map_tiles[index] = DungeonTile.new(true, false)
                elseif tile_value == 64 then
                    map_tiles[index] = DungeonTile.new(true, false)
                    spawn_point_x = x
                    spawn_point_y = y
                end
            end
            room_x = 0
            room_y = room_y + 1
        end

    end 


    local map = 
    {
        size_x = a_size_x,
        size_y = a_size_y,
        spawn_x = spawn_point_x,
        spawn_y = spawn_point_y,
        tiles = map_tiles
    }
    setmetatable(map, DungeonMap)
    return map
end

function DungeonMap:Function(a_move)
    
end

return {
    DungeonRoom = DungeonRoom,
    DungeonTile = DungeonTile,
    DungeonMap = DungeonMap
}
