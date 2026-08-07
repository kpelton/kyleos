-- Lunar Lander. Enter thrust 0..10 each turn.
local altitude, velocity, fuel = 100, 0, 60
while altitude > 0 do
  print(string.format("altitude=%d velocity=%d fuel=%d", altitude, velocity, fuel))
  io.write("thrust (0-10)> ")
  local thrust = tonumber(io.read()) or 0
  if thrust < 0 then thrust = 0 elseif thrust > 10 then thrust = 10 end
  if thrust > fuel then thrust = fuel end
  fuel = fuel - thrust
  velocity = velocity + 2 - thrust
  altitude = altitude - velocity
end
if velocity <= 3 then print("Soft landing. Welcome home!") else print("You crashed at velocity " .. velocity) end
