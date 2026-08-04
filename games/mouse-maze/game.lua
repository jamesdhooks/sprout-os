local screen_width, screen_height = 320, 240
local source_tile_size = 16
local padding_cells = 1
local mouse_body_extent = 95
local cheese_visible_extent = 174
local mouse_cell_coverage = 0.78
local cheese_cell_coverage = 0.75
local columns, rows, tile_size = 5, 3, 45
local frame_x, frame_y = 2, 7
local origin_x, origin_y = 47, 52
local character_scale = (tile_size * mouse_cell_coverage) / mouse_body_extent
local object_scale = (tile_size * cheese_cell_coverage) / cheese_visible_extent
local celebration_duration = 90
local movement_duration = 6
local campaign_level_limit = 1000
local rich_direction = {up = "north", right = "east", down = "south", left = "west"}

local grid = {}
local tiles = ""
local floor_tiles = ""
local level = 1
local campaign_seed = 1
local random_state = 1
local mouse_x, mouse_y = 1, 1
local goal_x, goal_y = 1, 1
local direction = "down"
local complete = false
local completion_ticks = 0
local move_from_x, move_from_y = 1, 1
local movement_tick = movement_duration
local queued_direction = nil
local previous = {}

local level_bands = {
  {through = 1, columns = 5, rows = 3},
  {through = 3, columns = 7, rows = 5},
  {through = 6, columns = 9, rows = 7},
  {through = 10, columns = 11, rows = 9},
  {through = 15, columns = 13, rows = 9},
  {through = 24, columns = 15, rows = 11},
  {through = 39, columns = 17, rows = 13},
  {through = 2147483647, columns = 19, rows = 15}
}

local function apply_level_band(next_level)
  local selected = level_bands[#level_bands]
  for _, band in ipairs(level_bands) do
    if next_level <= band.through then
      selected = band
      break
    end
  end
  columns, rows = selected.columns, selected.rows
  local framed_columns = columns + padding_cells * 2
  local framed_rows = rows + padding_cells * 2
  tile_size = math.max(8, math.floor(math.min(
      screen_width / framed_columns, screen_height / framed_rows)))
  frame_x = math.floor((screen_width - framed_columns * tile_size) / 2)
  frame_y = 0
  origin_x = frame_x + padding_cells * tile_size
  origin_y = frame_y + padding_cells * tile_size
  character_scale =
      (tile_size * mouse_cell_coverage) / mouse_body_extent
  object_scale =
      (tile_size * cheese_cell_coverage) / cheese_visible_extent
end

local function seed_random(value)
  random_state = value % 2147483647
  if random_state <= 0 then random_state = 1 end
end

local function random(maximum)
  random_state = (random_state * 48271) % 2147483647
  return (random_state % maximum) + 1
end

local function set_tile(x, y, value)
  grid[y + 1][x + 1] = value
end

local function tile_at(x, y)
  return grid[y + 1][x + 1]
end

local function build_tiles()
  local result = {}
  for y = 0, rows - 1 do
    for x = 0, columns - 1 do
      result[#result + 1] = string.char(tile_at(x, y))
    end
  end
  tiles = table.concat(result)
  floor_tiles = string.rep(string.char(0), #tiles)
end

local function carve_maze()
  grid = {}
  for y = 0, rows - 1 do
    grid[y + 1] = {}
    for x = 0, columns - 1 do grid[y + 1][x + 1] = 1 end
  end

  local stack = {{x = 1, y = 1}}
  set_tile(1, 1, 0)
  while #stack > 0 do
    local current = stack[#stack]
    local candidates = {}
    local directions = {{x = 0, y = -2}, {x = 2, y = 0},
                        {x = 0, y = 2}, {x = -2, y = 0}}
    for _, step in ipairs(directions) do
      local next_x, next_y = current.x + step.x, current.y + step.y
      if next_x > 0 and next_y > 0 and
          next_x < columns - 1 and next_y < rows - 1 and
          tile_at(next_x, next_y) == 1 then
        candidates[#candidates + 1] = {x = next_x, y = next_y,
          wall_x = current.x + step.x / 2,
          wall_y = current.y + step.y / 2}
      end
    end
    if #candidates == 0 then
      table.remove(stack)
    else
      local chosen = candidates[random(#candidates)]
      set_tile(chosen.wall_x, chosen.wall_y, 0)
      set_tile(chosen.x, chosen.y, 0)
      stack[#stack + 1] = {x = chosen.x, y = chosen.y}
    end
  end
end

local function choose_goal()
  local queue = {{x = 1, y = 1}}
  local distances = {[1 * columns + 1] = 0}
  local head = 1
  goal_x, goal_y = 1, 1
  local greatest_distance = 0
  while head <= #queue do
    local current = queue[head]
    head = head + 1
    local distance = distances[current.y * columns + current.x]
    if distance > greatest_distance then
      greatest_distance = distance
      goal_x, goal_y = current.x, current.y
    end
    for _, step in ipairs({{x = 0, y = -1}, {x = 1, y = 0},
                           {x = 0, y = 1}, {x = -1, y = 0}}) do
      local next_x, next_y = current.x + step.x, current.y + step.y
      local key = next_y * columns + next_x
      if next_x >= 0 and next_y >= 0 and next_x < columns and next_y < rows and
          tile_at(next_x, next_y) == 0 and distances[key] == nil then
        distances[key] = distance + 1
        queue[#queue + 1] = {x = next_x, y = next_y}
      end
    end
  end
end

local function generate_level(next_level)
  level = next_level
  apply_level_band(level)
  seed_random((campaign_seed + level * 104729) % 2147483647)
  carve_maze()
  choose_goal()
  build_tiles()
  mouse_x, mouse_y, direction = 1, 1, "down"
  complete, completion_ticks = false, 0
  move_from_x, move_from_y = mouse_x, mouse_y
  movement_tick, queued_direction = movement_duration, nil
  previous = {}
  sprout.storage_set("current-level", level)
end

local function blocked(x, y)
  return x < 0 or y < 0 or x >= columns or y >= rows or tile_at(x, y) == 1
end

local function requested_direction(actions)
  if actions.up then return "up", 0, -1 end
  if actions.down then return "down", 0, 1 end
  if actions.left then return "left", -1, 0 end
  if actions.right then return "right", 1, 0 end
  return nil, 0, 0
end

local function complete_level()
  complete, completion_ticks = true, 0
  local solved = sprout.storage_get("solved", 0) + 1
  sprout.storage_set("solved", solved)
  sprout.emit("LevelCompleted", string.format("maze-%04d", level))
end

local function requested_level_skip(actions)
  if not actions.start or not actions.primary then return 0 end
  local function pressed(direction)
    return actions[direction] and
        not (previous.start and previous.primary and previous[direction])
  end
  if pressed("down") then return 100 end
  if pressed("up") then return 10 end
  if pressed("right") then return 1 end
  return 0
end

function init()
  campaign_seed = sprout.storage_get("campaign-seed", 0)
  if campaign_seed == 0 then
    campaign_seed = sprout.random(2147483646)
    sprout.storage_set("campaign-seed", campaign_seed)
  end
  generate_level(math.max(1, sprout.storage_get("current-level", 1)))
end

function update(actions)
  local level_skip = requested_level_skip(actions)
  if level_skip > 0 then
    generate_level(math.min(campaign_level_limit, level + level_skip))
    previous = actions
    return
  end

  if complete then
    completion_ticks = completion_ticks + 1
    local skip = completion_ticks >= 20 and actions.primary and not previous.primary
    if completion_ticks >= celebration_duration or skip then
      generate_level(level + 1)
    else
      previous = actions
    end
    return
  end

  local requested = requested_direction(actions)
  if requested ~= nil then
    queued_direction = requested
  else
    queued_direction = nil
  end

  if movement_tick < movement_duration then
    movement_tick = movement_tick + 1
    if movement_tick == movement_duration and
        mouse_x == goal_x and mouse_y == goal_y then
      complete_level()
      previous = actions
      return
    end
  end

  if movement_tick == movement_duration and queued_direction ~= nil then
    direction = queued_direction
    local _, queued_dx, queued_dy = requested_direction({
      up = direction == "up", down = direction == "down",
      left = direction == "left", right = direction == "right"
    })
    queued_direction = nil
    if not blocked(mouse_x + queued_dx, mouse_y + queued_dy) then
      move_from_x, move_from_y = mouse_x, mouse_y
      mouse_x, mouse_y = mouse_x + queued_dx, mouse_y + queued_dy
      movement_tick = 0
    end
  end
  previous = actions
end

function render()
  local progress = movement_tick / movement_duration
  local rendered_x = move_from_x + (mouse_x - move_from_x) * progress
  local rendered_y = move_from_y + (mouse_y - move_from_y) * progress
  sprout.rect(0, 0, screen_width, screen_height, 43, 75, 49)
  sprout.tilemap("maze.tiles", floor_tiles, columns, origin_x, origin_y,
      tile_size / source_tile_size)
  sprout.sprite_batch({
    {sprite = "rich.cheese-goal",
      x = math.floor(origin_x + (goal_x + 0.5) * tile_size + 0.5),
      y = math.floor(origin_y + (goal_y + 0.5) * tile_size + 0.5),
      scale = object_scale},
    {animation = "rich.mouse-walk-" .. rich_direction[direction],
      x = math.floor(origin_x + (rendered_x + 0.5) * tile_size + 0.5),
      y = math.floor(origin_y + (rendered_y + 0.5) * tile_size + 0.5),
      scale = character_scale}
  })
  sprout.tilemap("maze.tiles", tiles, columns, origin_x, origin_y,
      tile_size / source_tile_size, 0)

  local level_width = math.max(48, tile_size * 2)
  local level_x = math.floor((screen_width - level_width) / 2)
  sprout.rect(level_x, frame_y, level_width, tile_size, 255, 244, 211, 238)
  sprout.label("Level " .. level, level_x + 4, frame_y,
      level_width - 8, tile_size,
      82, 35, 65)
  if complete then
    sprout.rect(104, 102, 112, 36, 255, 226, 155, 246)
    sprout.label("Cheese!", 112, 108, 96, 24, 82, 35, 65)
  end
end

local function floor_near_center()
  local best_x, best_y, best_distance = 1, 1, 10000
  for y = 1, rows - 2 do
    for x = 1, columns - 2 do
      if tile_at(x, y) == 0 then
        local distance = math.abs(x - math.floor(columns / 2)) +
            math.abs(y - math.floor(rows / 2))
        if distance < best_distance then
          best_x, best_y, best_distance = x, y, distance
        end
      end
    end
  end
  return best_x, best_y
end

function capture_scenario(name)
  if name == "gameplay" then
    mouse_x, mouse_y = floor_near_center()
    direction, complete, completion_ticks = "right", false, 0
  elseif name == "gameplay-level-2" then
    generate_level(2)
    mouse_x, mouse_y = floor_near_center()
    direction, complete, completion_ticks = "right", false, 0
  elseif name == "win" then
    mouse_x, mouse_y = goal_x, goal_y
    direction, complete, completion_ticks = "right", true, 24
  else
    error("unsupported capture scenario: " .. name)
  end
  move_from_x, move_from_y = mouse_x, mouse_y
  movement_tick, queued_direction, previous = movement_duration, nil, {}
end

function snapshot()
  return string.format("level:%d:%d:%d:%d:%d:%s:%d:%d", level, mouse_x,
      mouse_y, goal_x, goal_y, direction, complete and 1 or 0, completion_ticks)
end
