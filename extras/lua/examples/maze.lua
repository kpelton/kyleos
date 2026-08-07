-- Generate and display a deterministic depth-first maze, then its solution.
math.randomseed(os.time())
local w,h=15,9
local grid={}
for y=1,h do grid[y]={}; for x=1,w do grid[y][x]="#" end end
local function carve(x,y)
  grid[y][x]=" "
  local dirs={{2,0},{-2,0},{0,2},{0,-2}}
  for i=#dirs,2,-1 do local j=math.random(i); dirs[i],dirs[j]=dirs[j],dirs[i] end
  for _,d in ipairs(dirs) do
    local nx,ny=x+d[1],y+d[2]
    if nx>1 and nx<w and ny>1 and ny<h and grid[ny][nx]=="#" then grid[y+d[2]/2][x+d[1]/2]=" "; carve(nx,ny) end
  end
end
carve(2,2); grid[2][1]="S"; grid[h-1][w]="E"
for y=1,h do print(table.concat(grid[y])) end
print("S is the entrance; E is the exit. Try regenerating for another maze.")
