local columns, rows = 15, 11
local origin_x, origin_y = 40, 48
local layout = {
  "###############",
  "#.....#.......#",
  "#.###.#.#####.#",
  "#.#...#.....#.#",
  "#.#.#######.#.#",
  "#.#.........#.#",
  "#.###########.#",
  "#.............#",
  "###.#######.###",
  "#.............#",
  "###############"
}

local tiles = ""
local rich_scale = 3 / 64
local rich_direction = {up = "north", right = "east", down = "south", left = "west"}
for row = 1, rows do
  for column = 1, columns do
    local wall = layout[row]:sub(column, column) == "#"
    tiles = tiles .. string.char(wall and 1 or 0)
  end
end

local mouse_x, mouse_y, direction, complete
local previous = {}

local function blocked(x, y)
  return x < 0 or y < 0 or x >= columns or y >= rows or
      layout[y + 1]:sub(x + 1, x + 1) == "#"
end

local function reset()
  mouse_x, mouse_y, direction, complete = 1, 1, "down", false
end

function init()
  reset()
end

function update(actions)
  local dx, dy = 0, 0
  if actions.up and not previous.up then dy, direction = -1, "up"
  elseif actions.down and not previous.down then dy, direction = 1, "down"
  elseif actions.left and not previous.left then dx, direction = -1, "left"
  elseif actions.right and not previous.right then dx, direction = 1, "right" end

  if not complete and not blocked(mouse_x + dx, mouse_y + dy) then
    mouse_x, mouse_y = mouse_x + dx, mouse_y + dy
    if mouse_x == 13 and mouse_y == 9 then
      complete = true
      local solved = sprout.storage_get("solved", 0) + 1
      sprout.storage_set("solved", solved)
      sprout.emit("LevelCompleted", "maze-001")
    end
  elseif complete and actions.primary and not previous.primary then
    reset()
  end
  previous = actions
end

function render()
  sprout.rect(0, 0, 320, 240, 15, 31, 24)
  sprout.rect(0, 0, 320, 36, 29, 55, 43)
  sprout.rect(36, 44, 248, 184, 70, 132, 84)
  sprout.tilemap("maze.tiles", tiles, columns, origin_x, origin_y)
  sprout.sprite_batch({
    {sprite = "rich.cheese-goal", x = origin_x + 13 * 16 + 8,
      y = origin_y + 10 * 16, scale = rich_scale},
    {animation = "rich.mouse-walk-" .. rich_direction[direction],
      x = origin_x + mouse_x * 16 + 8, y = origin_y + (mouse_y + 1) * 16,
      scale = rich_scale}
  })
  if complete then
    sprout.rect(104, 8, 112, 20, 239, 166, 60)
    sprout.sprite("rich.cheese-goal", 160, 28, rich_scale)
  end
end

function snapshot()
  return table.concat({mouse_x, mouse_y, direction, complete and 1 or 0}, ":")
end
