local columns, rows = 10, 8
local origin_x, origin_y = 80, 56
local layout = {
  "##########",
  "#........#",
  "#........#",
  "#..##....#",
  "#........#",
  "#........#",
  "#........#",
  "##########"
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

local buttons = {{x = 7, y = 2}, {x = 7, y = 5}}
local initial_crates = {{x = 4, y = 2}, {x = 5, y = 4}}
local player_x, player_y, crates, direction, complete
local previous = {}

local function crate_at(x, y)
  for index, crate in ipairs(crates) do
    if crate.x == x and crate.y == y then return index end
  end
end

local function button_at(x, y)
  for _, button in ipairs(buttons) do
    if button.x == x and button.y == y then return true end
  end
  return false
end

local function wall_at(x, y)
  return x < 0 or y < 0 or x >= columns or y >= rows or
      layout[y + 1]:sub(x + 1, x + 1) == "#"
end

local function reset()
  player_x, player_y, direction, complete = 2, 2, "down", false
  crates = {}
  for index, crate in ipairs(initial_crates) do
    crates[index] = {x = crate.x, y = crate.y}
  end
end

local function evaluate()
  for _, button in ipairs(buttons) do
    if not crate_at(button.x, button.y) then return end
  end
  complete = true
  sprout.storage_set("completed", 1)
  sprout.emit("LevelCompleted", "blocks-001")
end

function init()
  reset()
end

function update(actions)
  if complete and actions.primary and not previous.primary then
    reset()
    previous = actions
    return
  end
  if actions.secondary and not previous.secondary then
    reset()
    previous = actions
    return
  end
  local dx, dy = 0, 0
  if actions.up and not previous.up then dy, direction = -1, "up"
  elseif actions.down and not previous.down then dy, direction = 1, "down"
  elseif actions.left and not previous.left then dx, direction = -1, "left"
  elseif actions.right and not previous.right then dx, direction = 1, "right" end
  if not complete and (dx ~= 0 or dy ~= 0) then
    local target_x, target_y = player_x + dx, player_y + dy
    local crate_index = crate_at(target_x, target_y)
    if crate_index then
      local crate_x, crate_y = target_x + dx, target_y + dy
      if not wall_at(crate_x, crate_y) and not crate_at(crate_x, crate_y) then
        crates[crate_index].x, crates[crate_index].y = crate_x, crate_y
        player_x, player_y = target_x, target_y
        evaluate()
      end
    elseif not wall_at(target_x, target_y) then
      player_x, player_y = target_x, target_y
    end
  end
  previous = actions
end

function render()
  sprout.rect(0, 0, 320, 240, 15, 31, 24)
  sprout.rect(0, 0, 320, 36, 29, 55, 43)
  sprout.rect(76, 52, 168, 136, 70, 132, 84)
  sprout.tilemap("room.tiles", tiles, columns, origin_x, origin_y)
  local sprites = {}
  for _, button in ipairs(buttons) do
    sprites[#sprites + 1] = {
      sprite = crate_at(button.x, button.y) and "rich.button-down" or "rich.button-up",
      x = origin_x + button.x * 16 + 8, y = origin_y + (button.y + 1) * 16,
      scale = rich_scale
    }
  end
  for _, crate in ipairs(crates) do
    sprites[#sprites + 1] = {
      sprite = button_at(crate.x, crate.y) and "rich.crate-solved" or "rich.crate-idle",
      x = origin_x + crate.x * 16 + 8, y = origin_y + (crate.y + 1) * 16,
      scale = rich_scale
    }
  end
  sprites[#sprites + 1] = {animation = "rich.hero-walk-" .. rich_direction[direction],
      x = origin_x + player_x * 16 + 8, y = origin_y + (player_y + 1) * 16,
      scale = rich_scale}
  sprites[#sprites + 1] = {sprite = "retry", x = 16, y = 8}
  sprout.sprite_batch(sprites)
  if complete then sprout.rect(104, 8, 112, 20, 83, 198, 126) end
end

function snapshot()
  local values = {player_x, player_y, direction, complete and 1 or 0}
  for _, crate in ipairs(crates) do
    values[#values + 1] = crate.x
    values[#values + 1] = crate.y
  end
  return table.concat(values, ":")
end
