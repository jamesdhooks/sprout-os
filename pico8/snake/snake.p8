pico-8 cartridge // http://www.pico-8.com
version 43
__lua__
-- snake
-- sprout arcade pico-8 edition

-- BEGIN SHARED ARCADE CORE
-- sprout pico arcade lifecycle core
-- injected into standalone carts by tools/pico8_arcade_build.py

function arc_boot(save_id)
 cartdata(save_id)
 arc_screen="title"
 arc_ticks=0
 arc_card_ticks=0
 arc_card_kind=nil
 arc_release_b=true
end

function arc_tick()
 arc_ticks+=1
 if not btn(5) then arc_release_b=true end
end

function arc_pressed(button)
 return btnp(button)
end

function arc_begin_card(kind,frames)
 arc_screen="card"
 arc_card_kind=kind
 arc_card_ticks=frames or 72
end

function arc_update_card()
 arc_card_ticks-=1
 if arc_card_ticks<=0 or btnp(4) then
  arc_screen="title"
  arc_card_kind=nil
  arc_release_b=false
  return true
 end
 return false
end

function arc_pop_scale(age)
 local t=min(1,age/15)
 local overshoot=1.18
 return 1+(overshoot-1)*sin(t*.5)-(1-t)*(1-t)
end

function arc_center_text(text,y,color)
 print(text,64-#text*2,y,color)
end

function arc_panel(x,y,w,h,fill,edge)
 rectfill(x+2,y,x+w-3,y+h,fill)
 rectfill(x,y+2,x+w-1,y+h-3,fill)
 line(x+2,y,x+w-3,y,edge)
 line(x+2,y+h-1,x+w-3,y+h-1,edge)
 line(x,y+2,x,y+h-3,edge)
 line(x+w-1,y+2,x+w-1,y+h-3,edge)
 pset(x+1,y+1,edge) pset(x+w-2,y+1,edge)
 pset(x+1,y+h-2,edge) pset(x+w-2,y+h-2,edge)
end
-- END SHARED ARCADE CORE

arc_boot("sprout_snake_dev")
qa_capture="normal"

gw=16 gh=14 cs=7 gx=8 gy=27
goal=12 snake={} dir={1,0} queued={1,0}
fruit={12,7} score=0 best=dget(0) step=0 speed=9
card_age=0 seed=24731

function occupied(x,y,ignore_tail)
 local last=#snake-(ignore_tail and 1 or 0)
 for i=1,last do if snake[i][1]==x and snake[i][2]==y then return true end end
 return false
end

function put_fruit()
 local open={}
 for y=0,gh-1 do for x=0,gw-1 do
  if not occupied(x,y,false) then add(open,{x,y}) end
 end end
 fruit=open[flr(rnd(#open))+1] or {0,0}
end

function reset_run()
 srand(seed)
 snake={{5,7},{4,7},{3,7}}
 dir={1,0} queued={1,0}
 score=0 step=0 speed=9
 put_fruit()
 arc_screen="play"
end

function end_run(kind)
 if score>best then best=score dset(0,best) end
 card_age=0
 arc_begin_card(kind,kind=="win" and 96 or 32767)
end

function choose_direction()
 if btnp(0) and dir[1]!=1 then queued={-1,0}
 elseif btnp(1) and dir[1]!=-1 then queued={1,0}
 elseif btnp(2) and dir[2]!=1 then queued={0,-1}
 elseif btnp(3) and dir[2]!=-1 then queued={0,1} end
end

function advance_snake()
 dir=queued
 local nx=snake[1][1]+dir[1] local ny=snake[1][2]+dir[2]
 local eating=nx==fruit[1] and ny==fruit[2]
 if nx<0 or ny<0 or nx>=gw or ny>=gh or occupied(nx,ny,not eating) then
  end_run("fail") return
 end
 add(snake,{nx,ny},1)
 if eating then
  score+=1
  speed=max(5,9-flr(score/4))
  if score>=goal then end_run("win") else put_fruit() end
 else
  deli(snake,#snake)
 end
end

function _update60()
 arc_tick()
 if arc_screen=="title" then
  if arc_pressed(4) then reset_run() end
 elseif arc_screen=="play" then
  if arc_pressed(5) then arc_screen="title" return end
  choose_direction()
  step+=1
  if step>=speed then step=0 advance_snake() end
  -- hidden capture helpers: hold a + left/right
  if btn(4) and arc_pressed(1) then score=goal card_age=0 arc_begin_card("win",32767)
  elseif btn(4) and arc_pressed(0) then card_age=0 arc_begin_card("fail",32767) end
 else
  card_age+=1
  if arc_card_kind=="fail" then
   if arc_pressed(4) then reset_run()
   elseif arc_pressed(5) then arc_screen="title" end
  elseif arc_update_card() then
   snake={} score=0
  end
 end
end

function draw_stars()
 cls(1)
 for i=0,26 do
  local x=(i*47+13)%128 local y=(i*29+7)%128
  pset(x,y,(i%3==0) and 10 or 6)
 end
end

function draw_title_snake()
 local points={{25,75},{32,75},{39,75},{46,75},{53,75},{60,75},{67,68},{74,61},{81,61},{88,61},{95,61}}
 for i=2,#points do line(points[i-1][1],points[i-1][2],points[i][1],points[i][2],3) end
 for i=2,#points do circfill(points[i][1],points[i][2],5,i%2==0 and 11 or 3) end
 circfill(25,75,7,11)
 circfill(22,73,1,0) circfill(28,73,1,0)
 line(24,79,26,79,8)
 -- crown and prize fruit
 line(20,66,23,61,10) line(23,61,26,66,10) line(26,66,30,61,10) line(30,61,31,67,10)
 circfill(101,54,8,8) rectfill(99,44,102,49,4) pset(96,51,9)
end

function draw_title()
 draw_stars()
 for y=35,103,8 do line(8,y,119,y,2) end
 arc_panel(10,5,108,31,2,13)
 arc_center_text("STARLIGHT",11,10)
 arc_center_text("SNAKE",22,11)
 draw_title_snake()
 arc_panel(24,108,80,15,5,13)
 arc_center_text("❎  PLAY",113,7)
end

function terrain_color(x,y)
 local n=(x*17+y*31+seed)%13
 if n==0 then return 1 elseif n==1 then return 5 else return 2 end
end

function draw_board()
 rectfill(gx-2,gy-2,gx+gw*cs+1,gy+gh*cs+1,13)
 for y=0,gh-1 do for x=0,gw-1 do
  local sx=gx+x*cs local sy=gy+y*cs
  rectfill(sx,sy,sx+cs-1,sy+cs-1,terrain_color(x,y))
  if (x*9+y*7)%23==0 then pset(sx+2,sy+2,1) end
 end end
end

function draw_apple(x,y,big)
 local r=big and 12 or 3
 circfill(x,y,r,8) circfill(x+(big and 8 or 2),y,r,8)
 if big then circfill(x-4,y-5,3,14) else pset(x-1,y-1,14) end
 line(x,y-r,x+1,y-r-(big and 6 or 2),4)
end

function draw_snake()
 for i=#snake,2,-1 do
  local p=snake[i] local q=snake[i-1]
  local x=gx+p[1]*cs+3 local y=gy+p[2]*cs+3
  local x2=gx+q[1]*cs+3 local y2=gy+q[2]*cs+3
  line(x,y,x2,y2,i%2==0 and 11 or 3)
  circfill(x,y,3,i%2==0 and 11 or 3)
 end
 local h=snake[1]
 if h then
  local x=gx+h[1]*cs+3 local y=gy+h[2]*cs+3
  circfill(x,y,4,11)
  local ex=-dir[2]*2 local ey=dir[1]*2
  pset(x+dir[1]*2+ex,y+dir[2]*2+ey,0)
  pset(x+dir[1]*2-ex,y+dir[2]*2-ey,0)
 end
end

function draw_play()
 cls(1)
 print(score.."/"..goal,4,7,10)
 print("best:"..flr(best),83,7,6)
 draw_board()
 draw_apple(gx+fruit[1]*cs+2,gy+fruit[2]*cs+3,false)
 draw_snake()
end

function draw_card()
 draw_stars()
 local s=max(.05,arc_pop_scale(card_age))
 local w=flr(106*s) local h=flr(86*s)
 local x=64-flr(w/2) local y=64-flr(h/2)
 arc_panel(x,y,w,h,arc_card_kind=="win" and 2 or 5,arc_card_kind=="win" and 13 or 8)
 if s>.72 then
  if arc_card_kind=="win" then
   draw_apple(58,48,true)
   -- crown
   line(47,28,51,20,10) line(51,20,58,27,10)
   line(58,27,65,20,10) line(65,20,70,29,10)
   line(47,29,70,29,10)
   for i=0,11 do
    local a=i/12 local r=34+sin((arc_ticks+i)/8)*3
    pset(64+cos(a)*r,50+sin(a)*r,(i%4)+8)
   end
   arc_center_text("FRUIT FEAST!",76,10)
   arc_center_text("round complete",90,7)
  else
   arc_center_text("BONK!",43,8)
   arc_center_text("score "..score,59,10)
   arc_center_text("❎ retry   🅾️ back",82,7)
  end
 end
end

function _draw()
 if arc_screen=="title" then draw_title()
 elseif arc_screen=="play" then draw_play()
 else draw_card() end
end

if qa_capture=="gameplay" then reset_run() step=-9999
elseif qa_capture=="win" then
 reset_run() score=goal card_age=18 arc_begin_card("win",32767)
elseif qa_capture=="fail" then
 reset_run() score=7 card_age=18 arc_begin_card("fail",32767)
end

__gfx__
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
