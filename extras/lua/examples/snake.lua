-- Step-based Snake. Enter w/a/s/d then Return; q quits.
math.randomseed(os.time())
local w, h = 20, 10
local snake, dx, dy = {{10,5}, {9,5}, {8,5}}, 1, 0
local food = {math.random(w), math.random(h)}
local function occupied(x, y)
  for _, p in ipairs(snake) do if p[1] == x and p[2] == y then return true end end
end
local function draw()
  for y = 1, h do
    local row = {}
    for x = 1, w do
      row[#row+1] = occupied(x,y) and "O" or (food[1] == x and food[2] == y and "*" or ".")
    end
    print(table.concat(row))
  end
  print("length=" .. #snake .. "  w/a/s/d, q")
end
while true do
  draw()
  local c = io.read()
  if c == "q" then break end
  if c == "w" and dy == 0 then dx,dy=0,-1 elseif c == "s" and dy == 0 then dx,dy=0,1
  elseif c == "a" and dx == 0 then dx,dy=-1,0 elseif c == "d" and dx == 0 then dx,dy=1,0 end
  local head = {snake[1][1] + dx, snake[1][2] + dy}
  if head[1] < 1 or head[1] > w or head[2] < 1 or head[2] > h or occupied(head[1],head[2]) then
    print("Game over!"); break
  end
  table.insert(snake, 1, head)
  if head[1] == food[1] and head[2] == food[2] then
    repeat food = {math.random(w), math.random(h)} until not occupied(food[1], food[2])
  else table.remove(snake) end
end
