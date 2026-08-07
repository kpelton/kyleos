-- adventure.lua
-- Text-based adventure game in Lua

-- Game state
local player = {
    location = "foyer",
    inventory = {}
}

local rooms = {
    foyer = {
        description = "You are in the foyer of a spooky house. A door leads north to the hall.",
        exits = { north = "hall" },
        items = { "flashlight" }
    },
    hall = {
        description = "A long hallway stretches before you. There are doors to the east and west.",
        exits = { south = "foyer", east = "kitchen", west = "library" },
        items = {}
    },
    kitchen = {
        description = "The kitchen is messy. Something smells rotten. There's a knife here.",
        exits = { west = "hall" },
        items = { "knife" }
    },
    library = {
        description = "A dusty library filled with old books. You see a mysterious key on a table.",
        exits = { east = "hall" },
        items = { "key" }
    }
}

-- helper functions
local function has_item(item)
    for _, i in ipairs(player.inventory) do
        if i == item then return true end
    end
    return false
end

local function remove_item(item)
    for idx, i in ipairs(player.inventory) do
        if i == item then
            table.remove(player.inventory, idx)
            return
        end
    end
end

local function print_inventory()
    if #player.inventory == 0 then
        print("You are carrying nothing.")
    else
        print("You are carrying: " .. table.concat(player.inventory, ", "))
    end
end

local function look()
    local room = rooms[player.location]
    print("\n" .. room.description)
    if #room.items > 0 then
        print("You see: " .. table.concat(room.items, ", "))
    end
    print("Exits: " .. table.concat((function()
        local t = {}
        for k,_ in pairs(room.exits) do table.insert(t,k) end
        return t
    end)(), ", "))
end

local function move(dir)
    local room = rooms[player.location]
    if room.exits[dir] then
        player.location = room.exits[dir]
        look()
    else
        print("You can't go that way.")
    end
end

local function take(item)
    local room = rooms[player.location]
    for idx, i in ipairs(room.items) do
        if i == item then
            table.insert(player.inventory, i)
            table.remove(room.items, idx)
            print("You picked up the " .. item .. ".")
            return
        end
    end
    print("There's no " .. item .. " here.")
end

local function drop(item)
    if has_item(item) then
        table.insert(rooms[player.location].items, item)
        remove_item(item)
        print("You dropped the " .. item .. ".")
    else
        print("You don't have that item.")
    end
end

-- main game loop
print("Welcome to Lua Adventure!")
look()

while true do
    io.write("\n> ")
    local input = io.read()
    local command, arg = input:match("^(%S+)%s*(.*)$")
    command = command:lower()
    arg = arg:lower()

    if command == "quit" or command == "exit" then
        print("Goodbye!")
        break
    elseif command == "look" then
        look()
    elseif command == "inventory" or command == "i" then
        print_inventory()
    elseif command == "go" then
        move(arg)
    elseif command == "take" then
        take(arg)
    elseif command == "drop" then
        drop(arg)
    else
        print("Unknown command. Try: look, go <direction>, take <item>, drop <item>, inventory, quit")
    end
end
