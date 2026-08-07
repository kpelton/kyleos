-- Shell Quest: a small KyleOS-themed text adventure.
local place, have = "console", {}
local rooms = {
  console={"A serial console hums beside /dev/console.", {dev="device room", tmp="scratch room"}},
  dev={"You find a glowing zero-byte crystal.", {console="console"}, "zero"},
  tmp={"A temporary workshop contains an old floppy key.", {console="console", vault="vault"}, "key"},
  vault={"A locked vault guards the kernel manual.", {tmp="scratch room"}}
}
while true do
  local r=rooms[place]; print(r[1])
  if r[3] and not have[r[3]] then have[r[3]]=true; print("You acquired: "..r[3]) end
  if place=="vault" and have.key then print("You unlock the vault and win Shell Quest!"); break end
  io.write("go dev/tmp/console/vault, inventory, quit> ")
  local cmd=io.read()
  if cmd=="quit" then break elseif cmd=="inventory" then for k in pairs(have) do print(k) end
  elseif r[2][cmd] then place=cmd else print("You cannot go there.") end
end
