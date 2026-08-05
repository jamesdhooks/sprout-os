local screen_width, screen_height = sprout.surface_size()
local ui_scale = screen_width / 320
local function ui_pixels(value)
  return math.floor(value * ui_scale + 0.5)
end
local source_tile_size = 16
local padding_cells = 1
local mouse_body_extent = 95
local cheese_visible_extent = 174
local mouse_cell_coverage = 0.78
local cheese_cell_coverage = 0.75
local columns, rows = 5, 3
local cell_width, cell_height, actor_cell_size = screen_width / 7,
    screen_height / 5, screen_width / 7
local origin_x, origin_y = cell_width, cell_height
local character_scale =
    (actor_cell_size * mouse_cell_coverage) / mouse_body_extent
local object_scale =
    (actor_cell_size * cheese_cell_coverage) / cheese_visible_extent
local celebration_duration = 60
local celebration_skip_delay = 12
local movement_duration = 6
local campaign_level_limit = 1000
local hint_duration = 180
local hint_max_cells = 12
local level_indicator_x, level_indicator_y = ui_pixels(3), ui_pixels(3)
local level_indicator_height = ui_pixels(28)
local level_indicator_radius = ui_pixels(8)
local indicator_reservation_cell_threshold = ui_pixels(16)
local rectangular_room_limit = 63
local initial_missing_room_ratio = 0.04
local maximum_missing_room_ratio = 0.22
local rich_direction = {up = "north", right = "east", down = "south", left = "west"}

local grid = {}
local wall_tiles = ""
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
local hint_path = {}
local hint_age, hint_remaining = 0, 0
local previous = {}

local level_bands = {
  {through = 1, columns = 5, rows = 3},
  {through = 3, columns = 7, rows = 5},
  {through = 6, columns = 9, rows = 7},
  {through = 10, columns = 11, rows = 9},
  {through = 15, columns = 13, rows = 9},
  {through = 24, columns = 15, rows = 11},
  {through = 39, columns = 17, rows = 13},
  {through = 59, columns = 19, rows = 15},
  {through = 89, columns = 21, rows = 15},
  {through = 129, columns = 23, rows = 17},
  {through = 169, columns = 25, rows = 19},
  {through = 219, columns = 27, rows = 19},
  {through = 279, columns = 29, rows = 21},
  {through = 349, columns = 31, rows = 23},
  {through = 429, columns = 33, rows = 23},
  {through = 509, columns = 35, rows = 25},
  {through = 599, columns = 37, rows = 27},
  {through = 689, columns = 39, rows = 27},
  {through = 769, columns = 41, rows = 29},
  {through = 839, columns = 43, rows = 31},
  {through = 899, columns = 45, rows = 31},
  {through = 949, columns = 47, rows = 33},
  {through = 979, columns = 49, rows = 35},
  {through = 2147483647, columns = 53, rows = 39}
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
  cell_width = screen_width / framed_columns
  cell_height = screen_height / framed_rows
  actor_cell_size = math.min(cell_width, cell_height)
  origin_x = padding_cells * cell_width
  origin_y = padding_cells * cell_height
  character_scale =
      (actor_cell_size * mouse_cell_coverage) / mouse_body_extent
  object_scale =
      (actor_cell_size * cheese_cell_coverage) / cheese_visible_extent
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

local function level_indicator_width()
  return math.max(ui_pixels(36), ui_pixels(18 + #tostring(level) * 11))
end

local function indicator_reservation_bounds()
  if cell_width >= indicator_reservation_cell_threshold and
      cell_height >= indicator_reservation_cell_threshold then
    return 0, 0
  end
  return level_indicator_x + level_indicator_width() + cell_width,
      level_indicator_y + level_indicator_height + cell_height
end

local function build_tiles()
  local terrain = ""
  for y = 0, rows - 1 do
    local row = grid[y + 1]
    for x = 0, columns - 1 do
      local tile = row[x + 1]
      if tile == 2 and x % 2 == 0 and y % 2 == 0 and
          ((x > 0 and row[x] == 1) or
           (x < columns - 1 and row[x + 2] == 1) or
           (y > 0 and grid[y][x + 1] == 1) or
           (y < rows - 1 and grid[y + 2][x + 1] == 1)) then
        tile = 1
      end
      terrain = terrain .. string.char(tile)
    end
  end
  wall_tiles = string.gsub(terrain, string.char(2), string.char(0))
  floor_tiles = string.gsub(terrain,
      "[" .. string.char(1) .. string.char(2) .. "]", string.char(1))
end

local function build_room_footprint()
  local room_columns = math.floor((columns - 1) / 2)
  local room_rows = math.floor((rows - 1) / 2)
  local total_rooms = room_columns * room_rows
  local maximum_rooms = 26 * 19
  local footprint_progress = math.max(0, math.min(1,
      (total_rooms - rectangular_room_limit) /
      (maximum_rooms - rectangular_room_limit)))
  local missing_ratio = 0
  if footprint_progress > 0 then
    missing_ratio = initial_missing_room_ratio + footprint_progress *
        (maximum_missing_room_ratio - initial_missing_room_ratio)
  end
  local allowed = {}
  local allowed_count = 0
  local reservation_width, reservation_height =
      indicator_reservation_bounds()

  local function room_key(room_x, room_y)
    return room_y * room_columns + room_x
  end

  for room_y = 0, room_rows - 1 do
    for room_x = 0, room_columns - 1 do
      local grid_x, grid_y = room_x * 2 + 1, room_y * 2 + 1
      local envelope_left = origin_x + (grid_x - 1) * cell_width
      local envelope_top = origin_y + (grid_y - 1) * cell_height
      local overlaps_indicator = reservation_width > 0 and
          envelope_left < reservation_width and
          envelope_top < reservation_height
      if not overlaps_indicator then
        allowed[room_key(room_x, room_y)] = true
        allowed_count = allowed_count + 1
      end
    end
  end

  if allowed_count == total_rooms and missing_ratio == 0 then
    return allowed, 0, 0, room_columns, room_rows
  end

  local target_count = math.max(1,
      allowed_count - math.floor(allowed_count * missing_ratio + 0.5))
  local target_x = math.floor(room_columns / 2) +
      random(math.max(1, math.floor(room_columns / 3))) -
      math.ceil(math.max(1, math.floor(room_columns / 3)) / 2)
  local target_y = math.floor(room_rows / 2) +
      random(math.max(1, math.floor(room_rows / 3))) -
      math.ceil(math.max(1, math.floor(room_rows / 3)) / 2)
  local seed_x, seed_y, seed_distance = 0, 0, 2147483647
  for room_y = 0, room_rows - 1 do
    for room_x = 0, room_columns - 1 do
      if allowed[room_key(room_x, room_y)] then
        local distance = math.abs(room_x - target_x) +
            math.abs(room_y - target_y)
        if distance < seed_distance then
          seed_x, seed_y, seed_distance = room_x, room_y, distance
        end
      end
    end
  end

  local active = {}
  local frontier, frontier_lookup = {}, {}
  local function add_frontier(room_x, room_y)
    if room_x < 0 or room_y < 0 or room_x >= room_columns or
        room_y >= room_rows then return end
    local key = room_key(room_x, room_y)
    if allowed[key] and not active[key] and not frontier_lookup[key] then
      frontier[#frontier + 1] = {x = room_x, y = room_y, key = key}
      frontier_lookup[key] = true
    end
  end
  local function activate(room_x, room_y)
    local key = room_key(room_x, room_y)
    active[key] = true
    add_frontier(room_x, room_y - 1)
    add_frontier(room_x + 1, room_y)
    add_frontier(room_x, room_y + 1)
    add_frontier(room_x - 1, room_y)
  end

  activate(seed_x, seed_y)
  local active_count = 1
  while active_count < target_count and #frontier > 0 do
    local index = random(#frontier)
    local chosen = frontier[index]
    frontier[index] = frontier[#frontier]
    table.remove(frontier)
    frontier_lookup[chosen.key] = nil
    if not active[chosen.key] then
      activate(chosen.x, chosen.y)
      active_count = active_count + 1
    end
  end
  return active, seed_x, seed_y, room_columns, room_rows
end

local function carve_maze()
  local active, seed_room_x, seed_room_y, room_columns, room_rows =
      build_room_footprint()
  local function active_room_at(room_x, room_y)
    if room_x < 0 or room_y < 0 or room_x >= room_columns or
        room_y >= room_rows then return false end
    return active[room_y * room_columns + room_x] == true
  end
  local function room_active(grid_x, grid_y)
    local room_x, room_y = (grid_x - 1) / 2, (grid_y - 1) / 2
    return active_room_at(room_x, room_y)
  end

  grid = {}
  for y = 0, rows - 1 do
    grid[y + 1] = {}
    for x = 0, columns - 1 do grid[y + 1][x + 1] = 2 end
  end
  for room_y = 0, room_rows - 1 do
    for room_x = 0, room_columns - 1 do
      local center_x, center_y = room_x * 2 + 1, room_y * 2 + 1
      if room_active(center_x, center_y) then
        set_tile(center_x, center_y, 1)
        set_tile(center_x, center_y - 1, 1)
        set_tile(center_x + 1, center_y, 1)
        set_tile(center_x, center_y + 1, 1)
        set_tile(center_x - 1, center_y, 1)
      end
    end
  end
  local seed_x, seed_y = seed_room_x * 2 + 1, seed_room_y * 2 + 1
  local stack = {{x = seed_x, y = seed_y}}
  set_tile(seed_x, seed_y, 0)
  while #stack > 0 do
    local current = stack[#stack]
    local candidates = {}
    local directions = {{x = 0, y = -2}, {x = 2, y = 0},
                        {x = 0, y = 2}, {x = -2, y = 0}}
    for _, step in ipairs(directions) do
      local next_x, next_y = current.x + step.x, current.y + step.y
      if next_x > 0 and next_y > 0 and
          next_x < columns - 1 and next_y < rows - 1 and
          room_active(next_x, next_y) and tile_at(next_x, next_y) == 1 then
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

local function choose_start()
  local cells = {}
  for y = 1, rows - 2, 2 do
    for x = 1, columns - 2, 2 do
      if tile_at(x, y) == 0 then
        cells[#cells + 1] = {x = x, y = y}
      end
    end
  end
  local chosen = cells[random(#cells)]
  mouse_x, mouse_y = chosen.x, chosen.y

  local exits = {}
  for _, candidate in ipairs({
      {name = "up", x = 0, y = -1}, {name = "right", x = 1, y = 0},
      {name = "down", x = 0, y = 1}, {name = "left", x = -1, y = 0}}) do
    if tile_at(mouse_x + candidate.x, mouse_y + candidate.y) == 0 then
      exits[#exits + 1] = candidate.name
    end
  end
  direction = exits[random(#exits)]
end

local function choose_goal()
  local queue = {{x = mouse_x, y = mouse_y}}
  local distances = {[mouse_y * columns + mouse_x] = 0}
  local head = 1
  goal_x, goal_y = mouse_x, mouse_y
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

local function solve_hint_path()
  local queue = {{x = mouse_x, y = mouse_y}}
  local predecessors = {}
  local visited = {[mouse_y * columns + mouse_x] = true}
  local head = 1
  while head <= #queue do
    local current = queue[head]
    head = head + 1
    if current.x == goal_x and current.y == goal_y then break end
    for _, step in ipairs({{x = 0, y = -1}, {x = 1, y = 0},
                           {x = 0, y = 1}, {x = -1, y = 0}}) do
      local next_x, next_y = current.x + step.x, current.y + step.y
      local key = next_y * columns + next_x
      if next_x >= 0 and next_y >= 0 and next_x < columns and next_y < rows and
          tile_at(next_x, next_y) == 0 and not visited[key] then
        visited[key] = true
        predecessors[key] = {x = current.x, y = current.y}
        queue[#queue + 1] = {x = next_x, y = next_y}
      end
    end
  end

  local reverse = {}
  local cursor = {x = goal_x, y = goal_y}
  while cursor.x ~= mouse_x or cursor.y ~= mouse_y do
    reverse[#reverse + 1] = cursor
    cursor = predecessors[cursor.y * columns + cursor.x]
    if cursor == nil then return {} end
  end

  local limit = math.min(hint_max_cells, math.max(1, math.floor(#reverse / 2)))
  local result = {}
  for offset = 1, limit do
    result[#result + 1] = reverse[#reverse - offset + 1]
  end
  return result
end

local function show_hint()
  hint_path = solve_hint_path()
  hint_age, hint_remaining = 0, hint_duration
end

local function generate_level(next_level)
  level = next_level
  apply_level_band(level)
  seed_random((campaign_seed + level * 104729) % 2147483647)
  carve_maze()
  choose_start()
  choose_goal()
  build_tiles()
  complete, completion_ticks = false, 0
  move_from_x, move_from_y = mouse_x, mouse_y
  movement_tick, queued_direction = movement_duration, nil
  hint_path = {}
  hint_age, hint_remaining = 0, 0
  previous = {}
  sprout.storage_set("current-level", level)
end

local function blocked(x, y)
  return x < 0 or y < 0 or x >= columns or y >= rows or tile_at(x, y) ~= 0
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

local function hint_input(actions)
  local chord_held = actions.start and actions.primary and actions.left
  local previous_chord = previous.start and previous.primary and previous.left
  local held = actions.secondary or chord_held
  local pressed = (actions.secondary and not previous.secondary) or
      (chord_held and not previous_chord)
  return held, pressed
end

function init()
  campaign_seed = sprout.storage_get("campaign-seed", 0)
  if campaign_seed == 0 then
    campaign_seed = sprout.random(2147483646)
    sprout.storage_set("campaign-seed", campaign_seed)
  end
  generate_level(math.max(1, sprout.storage_get("current-level", 1)))
end

function title_status()
  return "Level " .. level
end

function reset_progress()
  sprout.storage_set("solved", 0)
  generate_level(1)
end

function update(actions)
  local level_skip = requested_level_skip(actions)
  if level_skip > 0 then
    generate_level(math.min(campaign_level_limit, level + level_skip))
    previous = actions
    return
  end

  local hint_held, hint_pressed = hint_input(actions)
  if hint_held then
    if hint_pressed then show_hint() end
    previous = actions
    return
  end

  if hint_remaining > 0 then
    hint_age = hint_age + 1
    hint_remaining = hint_remaining - 1
  end

  if complete then
    completion_ticks = completion_ticks + 1
    local skip = completion_ticks >= celebration_skip_delay and
        actions.primary and not previous.primary
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
      hint_path, hint_remaining = {}, 0
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
      cell_width / source_tile_size, cell_height / source_tile_size, 1)
  if hint_remaining > 0 then
    local time_fade = math.min(1, hint_remaining / 30)
    for index, cell in ipairs(hint_path) do
      local distance_fade = 1 - 0.72 * ((index - 1) / math.max(1, #hint_path))
      local pulse = (math.sin((hint_age - index * 4) * 0.24) + 1) * 0.5
      local radius = math.max(2, math.min(5, math.floor(
          actor_cell_size * (0.075 + pulse * 0.025) + 0.5)))
      local shimmer = 0.72 + pulse * 0.28
      local alpha = math.floor(225 * distance_fade * time_fade * shimmer)
      local x = math.floor(origin_x + (cell.x + 0.5) * cell_width + 0.5)
      local y = math.floor(origin_y + (cell.y + 0.5) * cell_height + 0.5)
      sprout.circle(x, y, radius, 255, 188, 82, alpha)
      sprout.circle(x, y, math.max(1, radius - 2), 255, 247, 204, alpha)
    end
  end
  sprout.sprite_batch({
    {sprite = "rich.cheese-goal",
      x = math.floor(origin_x + (goal_x + 0.5) * cell_width + 0.5),
      y = math.floor(origin_y + (goal_y + 0.5) * cell_height + 0.5),
      scale = object_scale},
    {animation = "rich.mouse-walk-" .. rich_direction[direction],
      x = math.floor(origin_x + (rendered_x + 0.5) * cell_width + 0.5),
      y = math.floor(origin_y + (rendered_y + 0.5) * cell_height + 0.5),
      scale = character_scale}
  })
  sprout.tilemap("maze.tiles", wall_tiles, columns, origin_x, origin_y,
      cell_width / source_tile_size, cell_height / source_tile_size, 0)

  local level_text = tostring(level)
  local level_width = level_indicator_width()
  sprout.rounded_rect(level_indicator_x + 2, level_indicator_y + 3,
      level_width, level_indicator_height, level_indicator_radius,
      24, 55, 39, 175)
  sprout.rounded_rect(level_indicator_x, level_indicator_y,
      level_width, level_indicator_height, level_indicator_radius,
      245, 171, 65, 255)
  sprout.rounded_rect(level_indicator_x + 2, level_indicator_y + 2,
      level_width - 4, level_indicator_height - 4,
      level_indicator_radius - 2, 255, 239, 187, 255)
  sprout.display_label(level_text, level_indicator_x + 4,
      level_indicator_y + 3, level_width - 6, level_indicator_height - 2,
      201, 116, 53, 190)
  sprout.display_label(level_text, level_indicator_x + 3,
      level_indicator_y + 1, level_width - 6, level_indicator_height - 2,
      88, 38, 79)
  if complete then
    sprout.rounded_rect(ui_pixels(104), ui_pixels(102), ui_pixels(112),
        ui_pixels(36), ui_pixels(10), 255, 226, 155, 246)
    sprout.display_label("Cheese!", ui_pixels(112), ui_pixels(108),
        ui_pixels(96), ui_pixels(24), 82, 35, 65)
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
  if name == "generated-start" then
    -- Preserve the deterministic start selected during normal initialization.
  elseif name == "generated-level-2" then
    generate_level(2)
  elseif name == "generated-level-100" then
    generate_level(100)
  elseif name == "generated-level-1000" then
    generate_level(1000)
  elseif name == "hint" then
    show_hint()
    hint_age = 18
  elseif name == "hint-level-1000" then
    generate_level(1000)
    show_hint()
    hint_age = 18
  elseif name == "gameplay" then
    mouse_x, mouse_y = floor_near_center()
    direction, complete, completion_ticks = "right", false, 0
  elseif name == "gameplay-level-2" then
    generate_level(2)
    mouse_x, mouse_y = floor_near_center()
    direction, complete, completion_ticks = "right", false, 0
  elseif name == "gameplay-level-1000" then
    generate_level(1000)
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
