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

function DungeonMapViaFiles(a_map_size_x, a_map_size_y, a_room_names)
    spawn_x, a_spawn_y, entity = CreateMapTilesFromFiles(a_map_size_x, a_map_size_y, a_room_names)
    room = DungeonMap.new(a_map_size_x, a_map_size_y, nil, spawn_x, spawn_y)
    return room
end

function GetIFromXY(a_x, a_y, a_max_x)
    return a_x + a_y * a_max_x
end

return {
    DungeonMap = DungeonMap
}
