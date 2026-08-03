local width = 20
local height = 14
local cell = 14
local board_x = 20
local board_y = 36
local move_interval = 8

local glyphs = {
  ["0"] = "111101101101111", ["1"] = "010110010010111",
  ["2"] = "111001111100111", ["3"] = "111001111001111",
  ["4"] = "101101111001001", ["5"] = "111100111001111",
  ["6"] = "111100111101111", ["7"] = "111001001001001",
  ["8"] = "111101111101111", ["9"] = "111101111001111",
  A = "010101111101101", B = "110101110101110",
  C = "111100100100111", D = "110101101101110",
  E = "111100110100111", K = "101101110101101",
  N = "101111111111101", O = "111101101101111",
  P = "110101110100100", R = "110101110101101",
  S = "111100111001111", T = "111010010010010",
  U = "101101101101111", V = "101101101101010",
  [" "] = "000000000000000"
}

local snake = {}
local direction = {x = 1, y = 0}
local queued = {x = 1, y = 0}
local fruit = {x = 0, y = 0}
local ticks = 0
local score = 0
local best = 0
local ended = false
local restart_down = false

local function draw_text(text, x, y, scale, red, green, blue)
  for index = 1, #text do
    local glyph = glyphs[string.sub(text, index, index)] or glyphs[" "]
    for pixel = 1, 15 do
      if string.sub(glyph, pixel, pixel) == "1" then
        local column = (pixel - 1) % 3
        local row = math.floor((pixel - 1) / 3)
        sprout.rect(x + column * scale, y + row * scale, scale, scale,
                    red, green, blue)
      end
    end
    x = x + scale * 4
  end
end

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
  sprout.rect(0, 0, 320, 240, 17, 31, 26)
  sprout.rect(0, 0, 320, 29, 29, 55, 43)
  draw_text("SPROUT SNAKE", 8, 8, 2, 119, 225, 158)
  draw_text("SCORE " .. string.format("%02d", score), 176, 5, 2, 242, 214, 133)
  draw_text("BEST " .. string.format("%02d", best), 200, 17, 1, 166, 197, 175)

  sprout.rect(board_x - 2, board_y - 2, width * cell + 4, height * cell + 4,
              50, 83, 65)
  sprout.rect(board_x, board_y, width * cell, height * cell, 21, 41, 33)
  sprout.rect(board_x + fruit.x * cell + 3, board_y + fruit.y * cell + 3,
              8, 8, 242, 166, 75)
  for index, part in ipairs(snake) do
    local shade = ended and 105 or math.max(120, 226 - index * 5)
    sprout.rect(board_x + part.x * cell + 1, board_y + part.y * cell + 1,
                12, 12, 79, shade, ended and 116 or 139)
  end

  if ended then
    sprout.rect(72, 101, 176, 47, 29, 55, 43)
    sprout.rect(74, 103, 172, 43, 15, 27, 23)
    draw_text("ROUND OVER", 84, 109, 2, 242, 214, 133)
    draw_text("PRESS A", 116, 132, 1, 166, 197, 175)
  end
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
