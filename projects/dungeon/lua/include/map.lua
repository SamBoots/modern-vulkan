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

function DungeonMap.new(a_size_x, a_size_y, a_rooms)
    local map = 
    {
        size_x = a_size_x,
        size_y = a_size_y,
        spawn_x = 0,
        spawn_y = 0,
        tiles = {}
    }
    setmetatable(map, DungeonMap)
    return map
end

function DungeonMap:Function(a_move)
    
end