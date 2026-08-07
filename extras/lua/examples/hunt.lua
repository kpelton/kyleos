-- Hunt the Wumpus. Commands: move N, shoot N, quit.
math.randomseed(os.time())
local caves = {{2,5,8},{1,3,10},{2,4,6},{3,5,7},{1,4,9},{3,7,10},{4,6,8},{1,7,9},{5,8,10},{2,6,9}}
local room, wumpus, arrows = 1, math.random(2,10), 3
while true do
  print("You are in cave " .. room .. ". Tunnels: " .. table.concat(caves[room], ",") .. ". arrows=" .. arrows)
  if room == wumpus then print("The Wumpus got you!"); break end
  io.write("move N | shoot N | quit> ")
  local verb, n = (io.read() or ""):match("(%a+)%s*(%d*)")
  n = tonumber(n)
  if verb == "quit" then break end
  local linked = false
  for _, exit in ipairs(caves[room]) do if exit == n then linked = true end end
  if not linked then print("That tunnel is not here.")
  elseif verb == "move" then room = n
  elseif verb == "shoot" then
    arrows = arrows - 1
    if n == wumpus then print("You slew the Wumpus!"); break end
    if arrows == 0 then print("No arrows left."); break end
    print("Missed.")
  else print("Unknown command.") end
end
