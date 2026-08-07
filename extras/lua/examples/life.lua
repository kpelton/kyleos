math.randomseed(os.time())

-- Grid size
local width = 50
local height = 20

-- Initialize grid with random values
local grid = {}
for i = 1, height do
  grid[i] = {}
  for j = 1, width do
    grid[i][j] = math.random() < 0.5 and 1 or 0
  end
end

-- Function to print grid
local function printGrid()
  for i = 1, height do
    for j = 1, width do
      if grid[i][j] == 1 then
        io.write("* ")
      else
        io.write(". ")
      end
    end
    io.write("\n")
  end
end

-- Function to count live neighbors
local function countLiveNeighbors(i, j)
  local count = 0
  for x = -1, 1 do
    for y = -1, 1 do
      if x == 0 and y == 0 then
        goto continue
      end
      local ni = i + x
      local nj = j + y
      if ni < 1 or ni > height or nj < 1 or nj > width then
        goto continue
      end
      count = count + grid[ni][nj]
      ::continue::
    end
  end
  return count
end

-- Keep the example finite and self-contained.  The first argument selects the
-- number of generations; default to 10 when it is omitted.
local generations = tonumber(arg[1]) or 10
if generations < 1 or generations ~= math.floor(generations) then
  error("usage: life.lua [positive generation count]")
end
for generation = 1, generations do
  io.write("Generation ", generation, "\n")
  printGrid()
  local newGrid = {}
  for i = 1, height do
    newGrid[i] = {}
    for j = 1, width do
      local liveNeighbors = countLiveNeighbors(i, j)
      if grid[i][j] == 1 then
        if liveNeighbors < 2 or liveNeighbors > 3 then
          newGrid[i][j] = 0
        else
          newGrid[i][j] = 1
        end
      else
        if liveNeighbors == 3 then
          newGrid[i][j] = 1
        else
          newGrid[i][j] = 0
        end
      end
    end
  end
  grid = newGrid
end
