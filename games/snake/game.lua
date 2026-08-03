local width = 20
local height = 14
local cell = 14
local board_x = 20
local board_y = 36
local move_interval = 8
local half_cell = 7
local rich_scale = cell / 256

local snake = {}
local direction = {x = 1, y = 0}
local queued = {x = 1, y = 0}
local fruit = {x = 0, y = 0}
local ticks = 0
local score = 0
local best = 0
local ended = false
local restart_down = false

local function occupies(x, y)
  for _, part in ipairs(snake) do
    if part.x == x and part.y == y then
      return true
    end
  end
  return false
end

local function place_fruit()
  local available = {}
  for y = 0, height - 1 do
    for x = 0, width - 1 do
      if not occupies(x, y) then
        table.insert(available, {x = x, y = y})
      end
    end
  end
  if #available > 0 then
    fruit = available[sprout.random(#available)]
  end
end

local function cardinal(dx, dy)
  if dy < 0 then return "north" end
  if dx > 0 then return "east" end
  if dy > 0 then return "south" end
  return "west"
end

local function body_sprite(index)
  if index == 1 then
    return "rich.snake-head-" .. cardinal(direction.x, direction.y)
  end
  if index == #snake then
    local previous_part = snake[index - 1]
    local tail = snake[index]
    return "rich.snake-tail-" .. cardinal(previous_part.x - tail.x,
                                           previous_part.y - tail.y)
  end
  local previous_part = snake[index - 1]
  local part = snake[index]
  local next_part = snake[index + 1]
  local first = cardinal(previous_part.x - part.x, previous_part.y - part.y)
  local second = cardinal(next_part.x - part.x, next_part.y - part.y)
  if first == "north" and second == "south" or
      first == "south" and second == "north" then
    return "rich.snake-body-vertical"
  end
  if first == "east" and second == "west" or
      first == "west" and second == "east" then
    return "rich.snake-body-horizontal"
  end
  local pair = first .. ":" .. second
  if pair == "north:east" or pair == "east:north" then return "rich.snake-corner-ne" end
  if pair == "east:south" or pair == "south:east" then return "rich.snake-corner-es" end
  if pair == "south:west" or pair == "west:south" then return "rich.snake-corner-sw" end
  return "rich.snake-corner-wn"
end

local function reset()
  snake = {{x = 5, y = 7}, {x = 4, y = 7}, {x = 3, y = 7}}
  direction = {x = 1, y = 0}
  queued = {x = 1, y = 0}
  ticks = 0
  score = 0
  ended = false
  place_fruit()
end

function init()
  best = sprout.storage_get("best-score", 0)
  reset()
end

local function choose_direction(actions)
  if actions.up and direction.y ~= 1 then
    queued = {x = 0, y = -1}
  elseif actions.down and direction.y ~= -1 then
    queued = {x = 0, y = 1}
  elseif actions.left and direction.x ~= 1 then
    queued = {x = -1, y = 0}
  elseif actions.right and direction.x ~= -1 then
    queued = {x = 1, y = 0}
  end
end

function update(actions)
  local restart = actions.primary or actions.start
  if ended then
    if restart and not restart_down then
      reset()
    end
    restart_down = restart
    return
  end

  choose_direction(actions)
  ticks = ticks + 1
  if ticks % move_interval ~= 0 then
    restart_down = restart
    return
  end

  direction = queued
  local head = {x = snake[1].x + direction.x, y = snake[1].y + direction.y}
  if head.x < 0 or head.y < 0 or head.x >= width or head.y >= height or
      occupies(head.x, head.y) then
    ended = true
    if score > best then
      best = score
      sprout.storage_set("best-score", best)
    end
    restart_down = restart
    return
  end

  table.insert(snake, 1, head)
  if head.x == fruit.x and head.y == fruit.y then
    score = score + 1
    if score == 5 then
      sprout.emit("AchievementUnlocked", "snake-five")
    end
    place_fruit()
  else
    table.remove(snake)
  end
  restart_down = restart
end

function render()
  sprout.rect(0, 0, 320, 240, 25, 22, 74)
  sprout.label("Snake", 8, 3, 108, 24, 255, 203, 62)
  sprout.label("Score " .. string.format("%02d", score), 176, 3, 136, 20, 248, 236, 201)
  sprout.label("Best " .. string.format("%02d", best), 216, 19, 96, 12, 173, 190, 232)

  sprout.rect(board_x - 2, board_y - 2, width * cell + 4, height * cell + 4,
              238, 92, 133)
  sprout.rect(board_x, board_y, width * cell, height * cell, 61, 83, 72)
  local sprites = {}
  for y = 0, height - 1 do
    for x = 0, width - 1 do
      sprites[#sprites + 1] = {sprite = "rich.grass-plain",
        x = board_x + x * cell + half_cell,
        y = board_y + y * cell + half_cell, scale = rich_scale}
    end
  end
  sprites[#sprites + 1] = {sprite = "rich.fruit-apple",
    x = board_x + fruit.x * cell + half_cell,
    y = board_y + fruit.y * cell + half_cell, scale = rich_scale}
  for index, part in ipairs(snake) do
    sprites[#sprites + 1] = {sprite = body_sprite(index),
      x = board_x + part.x * cell + half_cell,
      y = board_y + part.y * cell + half_cell, scale = rich_scale}
  end
  sprout.sprite_batch(sprites)

  if ended then
    sprout.rect(72, 101, 176, 47, 255, 219, 109)
    sprout.label("Round over", 82, 106, 156, 23, 86, 31, 82)
    sprout.label("Press A to try again", 82, 128, 156, 14, 25, 22, 74)
  end
end

function capture_scenario(name)
  if name == "gameplay" then
    snake = {
      {x = 10, y = 5}, {x = 9, y = 5}, {x = 8, y = 5},
      {x = 8, y = 6}, {x = 8, y = 7}, {x = 7, y = 7}, {x = 6, y = 7}
    }
    direction = {x = 1, y = 0}
    queued = {x = 1, y = 0}
    fruit = {x = 13, y = 5}
    ticks, score, best, ended = 32, 4, math.max(best, 7), false
  elseif name == "fail" then
    snake = {
      {x = 19, y = 5}, {x = 18, y = 5}, {x = 17, y = 5},
      {x = 16, y = 5}, {x = 15, y = 5}, {x = 14, y = 5}
    }
    direction = {x = 1, y = 0}
    queued = {x = 1, y = 0}
    fruit = {x = 5, y = 9}
    ticks, score, best, ended = 48, 7, math.max(best, 7), true
  else
    error("unsupported capture scenario: " .. name)
  end
  restart_down = false
end

function snapshot()
  local parts = {ended and "ended" or "playing", tostring(score),
                 tostring(best), tostring(fruit.x), tostring(fruit.y)}
  for _, part in ipairs(snake) do
    table.insert(parts, tostring(part.x))
    table.insert(parts, tostring(part.y))
  end
  return table.concat(parts, ":")
end
