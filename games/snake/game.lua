local width, height, cell = 20, 15, 32
local screen_width, screen_height = sprout.surface_size()
local half_cell, rich_scale = 16, cell / 256
local round_goal = 12
local snake, direction, queued, fruit, ticks, score, ended, won
local selected_mode, best_round, best_endless, completed_rounds
local previous_title, restart_down, card_ticks, eat_burst

local function occupies(x, y)
  for _, part in ipairs(snake) do if part.x == x and part.y == y then return true end end
  return false
end

local function place_fruit()
  local available = {}
  for y=0,height-1 do for x=0,width-1 do
    if not occupies(x,y) then available[#available+1]={x=x,y=y} end
  end end
  if #available>0 then fruit=available[sprout.random(#available)] end
end

local function cardinal(dx,dy)
  if dy<0 then return "north" elseif dx>0 then return "east" elseif dy>0 then return "south" end
  return "west"
end

local function body_sprite(index)
  if index==1 then return "rich.snake-head-"..cardinal(direction.x,direction.y) end
  if index==#snake then
    local before=snake[index-1] local tail=snake[index]
    return "rich.snake-tail-"..cardinal(before.x-tail.x,before.y-tail.y)
  end
  local before,part,after=snake[index-1],snake[index],snake[index+1]
  local first=cardinal(before.x-part.x,before.y-part.y)
  local second=cardinal(after.x-part.x,after.y-part.y)
  if (first=="north" and second=="south") or (first=="south" and second=="north") then return "rich.snake-body-vertical" end
  if (first=="east" and second=="west") or (first=="west" and second=="east") then return "rich.snake-body-horizontal" end
  local pair=first..":"..second
  if pair=="north:east" or pair=="east:north" then return "rich.snake-corner-ne" end
  if pair=="east:south" or pair=="south:east" then return "rich.snake-corner-es" end
  if pair=="south:west" or pair=="west:south" then return "rich.snake-corner-sw" end
  return "rich.snake-corner-wn"
end

local function active_best() return selected_mode=="round" and best_round or best_endless end
local function move_interval() return math.max(4,9-math.floor(score/4)) end

local function reset_run()
  snake={{x=6,y=7},{x=5,y=7},{x=4,y=7}}
  direction={x=1,y=0}; queued={x=1,y=0}
  ticks,score,ended,won,card_ticks,eat_burst=0,0,false,false,0,0
  restart_down=false
  place_fruit()
end

function init()
  if sprout.storage_get("schema",0)~=1 then
    sprout.storage_set("schema",1); sprout.storage_set("mode",0)
  end
  selected_mode=sprout.storage_get("mode",0)==0 and "round" or "endless"
  best_round=sprout.storage_get("best-round",0)
  best_endless=sprout.storage_get("best-endless",0)
  completed_rounds=sprout.storage_get("completed-rounds",0)
  previous_title={}
  reset_run()
end

function title_update(actions)
  if actions.left or actions.right then
    selected_mode=selected_mode=="round" and "endless" or "round"
    sprout.storage_set("mode",selected_mode=="round" and 0 or 1)
  end
end

function title_status()
  return (selected_mode=="round" and "Round" or "Endless").."  Best "..active_best()
end

function reset_progress()
  best_round,best_endless,completed_rounds=0,0,0
  sprout.storage_set("best-round",0); sprout.storage_set("best-endless",0)
  sprout.storage_set("completed-rounds",0); sprout.sfx("reset")
  reset_run()
end

local function choose_direction(actions)
  if actions.up and direction.y~=1 then queued={x=0,y=-1}
  elseif actions.down and direction.y~=-1 then queued={x=0,y=1}
  elseif actions.left and direction.x~=1 then queued={x=-1,y=0}
  elseif actions.right and direction.x~=-1 then queued={x=1,y=0} end
end

local function finish_run(is_win)
  ended,won,card_ticks=true,is_win,0
  if selected_mode=="round" then
    if score>best_round then best_round=score; sprout.storage_set("best-round",best_round) end
    if is_win then completed_rounds=completed_rounds+1; sprout.storage_set("completed-rounds",completed_rounds); sprout.emit("LevelCompleted","snake-round") end
  else
    if score>best_endless then best_endless=score; sprout.storage_set("best-endless",best_endless) end
  end
  sprout.sfx(is_win and "win" or "collision")
end

function update(actions)
  if ended then
    card_ticks=card_ticks+1
    local restart=actions.primary or actions.start
    if restart and not restart_down then reset_run() end
    restart_down=restart
    return
  end
  choose_direction(actions)
  ticks=ticks+1; if eat_burst>0 then eat_burst=eat_burst-1 end
  if ticks%move_interval()~=0 then return end
  direction=queued
  local head={x=snake[1].x+direction.x,y=snake[1].y+direction.y}
  if head.x<0 or head.y<0 or head.x>=width or head.y>=height or occupies(head.x,head.y) then finish_run(false); return end
  table.insert(snake,1,head)
  if head.x==fruit.x and head.y==fruit.y then
    score=score+1; eat_burst=18; sprout.sfx("eat")
    if score%4==0 then sprout.sfx("speed",0.7) end
    if selected_mode=="round" and score>=round_goal then finish_run(true); return end
    place_fruit()
  else table.remove(snake) end
end

local function draw_card()
  sprout.rect(0,0,screen_width,screen_height,22,19,56,180)
  local scale=math.min(1,card_ticks/14)
  local w,h=math.floor(360*scale),math.floor(240*scale)
  if w<40 then return end
  local x=math.floor((screen_width-w)/2)
  local y=math.floor((screen_height-h)/2+math.sin(card_ticks/8)*4)
  local fill=won and {255,222,112} or {143,40,85}
  sprout.rounded_rect(x,y,w,h,30,fill[1],fill[2],fill[3],248)
  sprout.display_label(won and "Garden glow!" or "Bump!",x+25,y+50,w-50,72,won and 53 or 247,won and 39 or 235,won and 104 or 201)
  sprout.label(won and "12 fruit - round complete" or "A Try again    B Home",x+28,y+145,w-56,34,won and 32 or 247,won and 27 or 235,won and 82 or 201)
end

function render()
  sprout.rect(0,0,screen_width,screen_height,32,27,82)
  local sprites={}
  for y=0,height-1 do for x=0,width-1 do
    local variant=(x*37+y*53+x*y*17+x*x*11+y*y*7)%97
    local id=variant<2 and "rich.grass-flower" or (variant==11 or variant==29) and "rich.grass-clover" or "rich.grass-plain"
    sprites[#sprites+1]={sprite=id,x=x*cell+half_cell,y=y*cell+half_cell,scale=rich_scale}
  end end
  sprites[#sprites+1]={animation=score%2==0 and "rich.apple-idle" or "rich.pear-idle",x=fruit.x*cell+half_cell,y=fruit.y*cell+half_cell,scale=rich_scale}
  for index,part in ipairs(snake) do sprites[#sprites+1]={sprite=body_sprite(index),x=part.x*cell+half_cell,y=part.y*cell+half_cell,scale=rich_scale} end
  sprout.sprite_batch(sprites)
  if eat_burst>0 then
    local alpha=math.floor(255*eat_burst/18)
    for i=0,5 do local angle=i*math.pi/3; sprout.circle(math.floor(snake[1].x*cell+16+math.cos(angle)*(22-eat_burst)),math.floor(snake[1].y*cell+16+math.sin(angle)*(22-eat_burst)),4,255,222,112,alpha) end
  end
  sprout.rounded_rect(8,8,136,34,12,22,19,56,225)
  sprout.display_label(string.format("%02d",score),16,11,48,28,255,222,112)
  sprout.label(selected_mode=="round" and "/ 12 Round" or "Endless",62,12,74,25,247,235,201)
  if ended then draw_card() end
end

function capture_scenario(name)
  if name=="gameplay" then snake={{x=10,y=5},{x=9,y=5},{x=8,y=5},{x=8,y=6},{x=8,y=7},{x=7,y=7}}; direction={x=1,y=0}; queued={x=1,y=0}; fruit={x=13,y=5}; score=4
  elseif name=="win" then selected_mode="round"; score=12; ended=true; won=true; card_ticks=18
  elseif name=="fail" then snake={{x=19,y=5},{x=18,y=5},{x=17,y=5}}; score=7; ended=true; won=false; card_ticks=18
  elseif name=="endless-gameplay" then selected_mode="endless"; score=18; snake={{x=10,y=5},{x=9,y=5},{x=8,y=5},{x=8,y=6},{x=8,y=7},{x=7,y=7},{x=6,y=7}}; fruit={x=14,y=8}
  else error("unsupported capture scenario: "..name) end
end

function snapshot()
  local parts={selected_mode,ended and "ended" or "playing",won and 1 or 0,score,best_round,best_endless,fruit.x,fruit.y}
  for _,part in ipairs(snake) do parts[#parts+1]=part.x; parts[#parts+1]=part.y end
  return table.concat(parts,":")
end
