function GetIFromXY(a_x, a_y, a_max_x)
    return a_x + a_y * a_max_x
end

local dungeon_dictionary = {}
dungeon_dictionary[64] = true
dungeon_dictionary[46] = true
dungeon_dictionary[35] = false;

local DungeonMap = {}
DungeonMap.__index = DungeonMap

function DungeonMap.new(a_size_x, a_size_y, a_tiles, a_entity, a_spawn_x, a_spawn_y)
    local room = 
    {
        size_x = a_size_x,
        size_y = a_size_y,
        tiles = a_tiles,
        entity = a_entity,
        spawn_x = a_spawn_x,
        spawn_y = a_spawn_y
    }
    setmetatable(room, DungeonMap)
    return room
end

function DungeonMap:TileWalkable(a_x, a_y)
    if a_x >= self.size_x or a_y >= self.size_y then
        print("out of bounds")
        return false
    end

    local tile = tiles[GetIFromXY(a_x, a_y, self.size_x)]
    return dungeon_dictionary[tile]
end

function DungeonMapViaFiles(a_map_size_x, a_map_size_y, a_room_names)
    tiles, spawn_x, spawn_y, entity = CreateMapTilesFromFiles(a_map_size_x, a_map_size_y, a_room_names)
    map = DungeonMap.new(a_map_size_x, a_map_size_y, tiles, entity, spawn_x, spawn_y)
    return map
end
 
return {
    DungeonMap = DungeonMap,
    dungeon_dictionary = dungeon_dictionary
}
