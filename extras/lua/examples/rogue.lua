-- Tiny serial-friendly roguelike. Commands: n, s, e, w, q.
math.randomseed(os.time())
local width, height = 8, 6
local px, py, hp, gold = 1, 1, 12, 0
local monsters = {{3,2,4}, {6,4,6}, {8,6,8}}

local function monster_at(x, y)
  for _, m in ipairs(monsters) do
    if m[1] == x and m[2] == y and m[3] > 0 then return m end
  end
end
local function draw()
  for y = 1, height do
    local row = {}
    for x = 1, width do
      local m = monster_at(x, y)
      row[#row + 1] = x == px and y == py and "@" or (m and "M" or ".")
    end
    print(table.concat(row))
  end
  print("hp=" .. hp .. " gold=" .. gold .. "  n/s/e/w, q")
end

while hp > 0 do
  draw()
  local cmd = io.read()
  if cmd == "q" then break end
  local dx, dy = 0, 0
  if cmd == "n" then dy = -1 elseif cmd == "s" then dy = 1
  elseif cmd == "e" then dx = 1 elseif cmd == "w" then dx = -1 end
  local nx, ny = px + dx, py + dy
  if nx >= 1 and nx <= width and ny >= 1 and ny <= height then
    local m = monster_at(nx, ny)
    if m then
      local hit = math.random(1, 4)
      m[3] = m[3] - hit
      print("You hit the monster for " .. hit)
      if m[3] <= 0 then gold = gold + 5; print("Monster defeated!")
      else hp = hp - math.random(1, 3) end
    else px, py = nx, ny end
  end
end
print(hp > 0 and "You leave the dungeon." or "You died in the dungeon.")
