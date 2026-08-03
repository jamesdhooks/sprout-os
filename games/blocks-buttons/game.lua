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
local character_scale = 5 / 32
local object_scale = 3 / 64
local rich_direction = {up = "north", right = "east", down = "south", left = "west"}
for row = 1, rows do
  for column = 1, columns do
    local wall = layout[row]:sub(column, column) == "#"
    tiles = tiles .. string.char(wall and 1 or 0)
  end
end

local buttons = {{x = 7, y = 2}, {x = 7, y = 5}}
local initial_crates = {{x = 4, y = 2}, {x = 5, y = 4}}
local player_x, player_y, crates, direction, complete, deadlocked
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
  player_x, player_y, direction, complete, deadlocked = 2, 2, "down", false, false
  crates = {}
  for index, crate in ipairs(initial_crates) do
    crates[index] = {x = crate.x, y = crate.y}
  end
end

local function evaluate()
  for _, button in ipairs(buttons) do
    if not crate_at(button.x, button.y) then
      for _, crate in ipairs(crates) do
        if not button_at(crate.x, crate.y) and
            (wall_at(crate.x - 1, crate.y) or wall_at(crate.x + 1, crate.y)) and
            (wall_at(crate.x, crate.y - 1) or wall_at(crate.x, crate.y + 1)) then
          deadlocked = true
          return
        end
      end
      return
    end
  end
  complete = true
  sprout.storage_set("completed", 1)
  sprout.emit("LevelCompleted", "blocks-001")
end

function init()
  reset()
end

function update(actions)
  if (complete or deadlocked) and actions.primary and not previous.primary then
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
  if not complete and not deadlocked and (dx ~= 0 or dy ~= 0) then
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
  sprout.rect(0, 0, 320, 240, 19, 58, 106)
  sprout.rect(58, 34, 204, 188, 194, 91, 58)
  sprout.rect(70, 46, 180, 164, 56, 139, 158)
  sprout.tilemap("room.tiles", tiles, columns, origin_x, origin_y)
  local sprites = {}
  for _, button in ipairs(buttons) do
    sprites[#sprites + 1] = {
      sprite = crate_at(button.x, button.y) and "rich.button-down" or "rich.button-up",
      x = origin_x + button.x * 16 + 8, y = origin_y + button.y * 16 + 8,
      scale = object_scale
    }
  end
  for _, crate in ipairs(crates) do
    sprites[#sprites + 1] = {
      sprite = button_at(crate.x, crate.y) and "rich.crate-solved" or "rich.crate-idle",
      x = origin_x + crate.x * 16 + 8, y = origin_y + crate.y * 16 + 8,
      scale = object_scale
    }
  end
  sprites[#sprites + 1] = {animation = "rich.hero-walk-" .. rich_direction[direction],
      x = origin_x + player_x * 16 + 8, y = origin_y + player_y * 16 + 8,
      scale = character_scale}
  sprout.sprite_batch(sprites)
  if complete then
    sprout.rect(84, 6, 152, 27, 255, 207, 61)
    sprout.label("Puzzle solved!", 92, 8, 136, 22, 15, 50, 94)
  elseif deadlocked then
    sprout.rect(66, 6, 188, 27, 255, 191, 171)
    sprout.label("No moves - A retry", 74, 8, 172, 22, 126, 39, 38)
  end
end

function capture_scenario(name)
  if name == "gameplay" then
    player_x, player_y, direction, complete, deadlocked = 6, 2, "right", false, false
    crates = {{x = 7, y = 2}, {x = 5, y = 4}}
  elseif name == "win" then
    player_x, player_y, direction, complete, deadlocked = 6, 5, "right", true, false
    crates = {{x = 7, y = 2}, {x = 7, y = 5}}
  elseif name == "fail" then
    player_x, player_y, direction, complete, deadlocked = 2, 2, "left", false, true
    crates = {{x = 1, y = 1}, {x = 5, y = 4}}
  elseif name == "deadlock-test" then
    player_x, player_y, direction, complete, deadlocked = 3, 1, "left", false, false
    crates = {{x = 2, y = 1}, {x = 5, y = 4}}
  else
    error("unsupported capture scenario: " .. name)
  end
  previous = {}
end

function snapshot()
  local values = {player_x, player_y, direction, complete and 1 or 0,
                  deadlocked and 1 or 0}
  for _, crate in ipairs(crates) do
    values[#values + 1] = crate.x
    values[#values + 1] = crate.y
  end
  return table.concat(values, ":")
end
