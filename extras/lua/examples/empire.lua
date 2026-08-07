-- Tiny 5x5 turn-based territory game. Claim a square: row column.
math.randomseed(os.time())
local n, board, turns = 5, {}, 12
for y=1,n do board[y]={}; for x=1,n do board[y][x]="." end end
local function draw()
  for y=1,n do print(table.concat(board[y], " ")) end
end
for turn=1,turns do
  draw(); io.write("turn "..turn.." claim row col> ")
  local r,c = (io.read() or ""):match("(%d+)%s+(%d+)")
  r,c=tonumber(r),tonumber(c)
  if r and c and r>=1 and r<=n and c>=1 and c<=n and board[r][c]=="." then board[r][c]="P" else print("Invalid claim.") end
  repeat r,c=math.random(n),math.random(n) until board[r][c]=="."
  board[r][c]="C"
end
draw()
local p,c=0,0
for y=1,n do for x=1,n do if board[y][x]=="P" then p=p+1 elseif board[y][x]=="C" then c=c+1 end end end
print("You="..p.." computer="..c..(p>c and " You win!" or " Computer wins."))
