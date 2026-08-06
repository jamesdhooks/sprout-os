local columns, rows = 10, 8
local screen_width, screen_height = sprout.surface_size()
local layout_scale = screen_width / 320
local function px(value)
  return math.floor(value * layout_scale + 0.5)
end
local origin_x, origin_y = px(80), px(56)
local cell = px(16)
local half_cell = math.floor(cell / 2)
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
local character_scale = (3 / 32) * layout_scale
local object_scale = (1 / 16) * layout_scale
local rich_direction = {up = "north", right = "west", down = "south", left = "west"}
for row = 1, rows do
  for column = 1, columns do
    local wall = layout[row]:sub(column, column) == "#"
    tiles = tiles .. string.char(wall and 1 or 0)
  end
end

local buttons = {{x = 7, y = 2}, {x = 7, y = 5}}
local initial_crates = {{x = 4, y = 2}, {x = 5, y = 4}}
local player_x, player_y, crates, direction, complete, deadlocked, moving
local previous = {}
local move_duration = 8

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
  moving = nil
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
  if moving then
    moving.tick = moving.tick + 1
    if moving.tick >= move_duration then
      moving = nil
      evaluate()
    end
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
        moving = {tick = 0, from_x = player_x, from_y = player_y,
            crate = crate_index, crate_from_x = target_x, crate_from_y = target_y,
            pushing = true}
        crates[crate_index].x, crates[crate_index].y = crate_x, crate_y
        player_x, player_y = target_x, target_y
      end
    elseif not wall_at(target_x, target_y) then
      moving = {tick = 0, from_x = player_x, from_y = player_y, pushing = false}
      player_x, player_y = target_x, target_y
    end
  end
  previous = actions
end

function render()
  sprout.rect(0, 0, screen_width, screen_height, 19, 58, 106)
  sprout.rect(px(58), px(34), px(204), px(188), 194, 91, 58)
  sprout.rect(px(70), px(46), px(180), px(164), 56, 139, 158)
  sprout.tilemap("room.tiles", tiles, columns, origin_x, origin_y,
      layout_scale, layout_scale)
  local sprites = {}
  for _, button in ipairs(buttons) do
    sprites[#sprites + 1] = {
      sprite = crate_at(button.x, button.y) and "rich.button-down" or "rich.button-up",
      x = origin_x + button.x * cell + half_cell,
      y = origin_y + (button.y + 1) * cell,
      scale = object_scale
    }
  end
  local progress = moving and math.min(1, moving.tick / move_duration) or 1
  for index, crate in ipairs(crates) do
    local crate_x, crate_y = crate.x, crate.y
    if moving and moving.crate == index then
      crate_x = moving.crate_from_x + (crate.x - moving.crate_from_x) * progress
      crate_y = moving.crate_from_y + (crate.y - moving.crate_from_y) * progress
    end
    sprites[#sprites + 1] = {
      sprite = button_at(crate.x, crate.y) and "rich.crate-solved" or "rich.crate-idle",
      x = origin_x + crate_x * cell + half_cell,
      y = origin_y + (crate_y + 1) * cell,
      scale = object_scale
    }
  end
  local render_x, render_y = player_x, player_y
  if moving then
    render_x = moving.from_x + (player_x - moving.from_x) * progress
    render_y = moving.from_y + (player_y - moving.from_y) * progress
  end
  local source_direction = rich_direction[direction]
  local sprite_name = "rich.hero-" .. source_direction .. "-02"
  if moving then
    if moving.pushing then
      sprite_name = "rich.hero-" .. source_direction .. "-push"
    else
      local frame = math.min(4, math.floor(progress * 4) + 1)
      sprite_name = string.format("rich.hero-%s-%02d", source_direction, frame)
    end
  end
  sprites[#sprites + 1] = {sprite = sprite_name,
      x = origin_x + render_x * cell + half_cell,
      y = origin_y + (render_y + 1) * cell,
      scale = character_scale,
      flipX = direction == "right"}
  sprout.sprite_batch(sprites)
  if complete then
    sprout.rounded_rect(px(84), px(6), px(152), px(27), px(8), 255, 207, 61)
    sprout.display_label("Puzzle solved!", px(92), px(8), px(136), px(22),
        15, 50, 94)
  elseif deadlocked then
    sprout.rounded_rect(px(66), px(6), px(188), px(27), px(8), 255, 191, 171)
    sprout.display_label("No moves - A retry", px(74), px(8), px(172), px(22),
        126, 39, 38)
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
