-- Turn-based Space Invaders. Commands: a, d, fire, q.
math.randomseed(os.time())
local px, score = 5, 0
local aliens = {{2,1},{4,1},{6,1},{8,1},{10,1},{3,2},{5,2},{7,2},{9,2}}
local function draw()
  for y = 1, 8 do
    local row = {}
    for x = 1, 12 do
      local mark = "."
      for _, a in ipairs(aliens) do if a[1] == x and a[2] == y then mark = "W" end end
      if y == 8 and x == px then mark = "A" end
      row[#row+1] = mark
    end
    print(table.concat(row))
  end
  print("score=" .. score .. "  a/d/fire/q")
end
while #aliens > 0 do
  draw()
  local cmd = io.read()
  if cmd == "q" then break end
  if cmd == "a" then px = math.max(1, px - 1) elseif cmd == "d" then px = math.min(12, px + 1)
  elseif cmd == "fire" then
    for i, a in ipairs(aliens) do
      if a[1] == px then table.remove(aliens, i); score = score + 10; print("Hit!"); break end
    end
  end
  for _, a in ipairs(aliens) do a[2] = a[2] + 1; if a[2] >= 8 then print("Invaded!"); os.exit() end end
end
if #aliens == 0 then print("Sector clear!") end
