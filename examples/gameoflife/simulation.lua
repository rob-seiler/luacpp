-- Conway's Game of Life
-- C++ side: Grid class handles cells, evolution, neighbor counting.
-- Lua side: pattern placement, simulation loop, rendering.

GENERATIONS = 40

grid = Grid(40, 15)

-- Glider (moves diagonally down-right)
local function placeGlider(g, ox, oy)
    g:set(ox + 1, oy + 0, true)
    g:set(ox + 2, oy + 1, true)
    g:set(ox + 0, oy + 2, true)
    g:set(ox + 1, oy + 2, true)
    g:set(ox + 2, oy + 2, true)
end

-- Blinker (oscillates between vertical and horizontal)
local function placeBlinker(g, ox, oy)
    g:set(ox, oy + 0, true)
    g:set(ox, oy + 1, true)
    g:set(ox, oy + 2, true)
end

-- Block (stable)
local function placeBlock(g, ox, oy)
    g:set(ox + 0, oy + 0, true)
    g:set(ox + 1, oy + 0, true)
    g:set(ox + 0, oy + 1, true)
    g:set(ox + 1, oy + 1, true)
end

placeGlider(grid, 2, 2)
placeBlinker(grid, 30, 6)
placeBlock(grid, 25, 11)

local function render(g)
    print("Generation " .. g.generation .. "  |  Alive: " .. g:countAlive() ..
          "  |  Grid: " .. g.width .. "x" .. g.height)
    io.write(tostring(g))
end

beginFrame()
render(grid)
for gen = 1, GENERATIONS do
    grid:step()
    beginFrame()
    render(grid)
end
