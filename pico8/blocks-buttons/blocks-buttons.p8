pico-8 cartridge // http://www.pico-8.com
version 43
__lua__
-- blocks & buttons
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

arc_boot("sprout_blocks_buttons_dev")
qa_capture="normal"

bw=8 bh=8 cs=12 bx=16 by=24
walls={
 "########",
 "#......#",
 "#......#",
 "#..#...#",
 "#..#...#",
 "#......#",
 "#......#",
 "########"
}
targets={{6,1},{6,5}}
start_crates={{3,1},{4,4}}
px=2 py=2 crates={} facing=1
moves=0 pushes=0 card_age=0

function reset_room()
 px=2 py=2 facing=1 moves=0 pushes=0
 crates={}
 for c in all(start_crates) do add(crates,{c[1],c[2]}) end
 arc_screen="play"
end

function wall(x,y)
 return x<0 or y<0 or x>=bw or y>=bh or sub(walls[y+1],x+1,x+1)=="#"
end

function crate_at(x,y)
 for i=1,#crates do
  if crates[i][1]==x and crates[i][2]==y then return i end
 end
 return 0
end

function target_at(x,y)
 for t in all(targets) do if t[1]==x and t[2]==y then return true end end
 return false
end

function room_solved()
 for t in all(targets) do if crate_at(t[1],t[2])==0 then return false end end
 return true
end

function deadlocked()
 for c in all(crates) do
  if not target_at(c[1],c[2]) then
   local h=wall(c[1]-1,c[2]) or wall(c[1]+1,c[2])
   local v=wall(c[1],c[2]-1) or wall(c[1],c[2]+1)
   if h and v then return true end
  end
 end
 return false
end

function move_player(dx,dy,face)
 facing=face
 local nx=px+dx local ny=py+dy
 if wall(nx,ny) then return end
 local ci=crate_at(nx,ny)
 if ci>0 then
  local cx=nx+dx local cy=ny+dy
  if wall(cx,cy) or crate_at(cx,cy)>0 then return end
  crates[ci][1]=cx crates[ci][2]=cy pushes+=1
 end
 px=nx py=ny moves+=1
 if room_solved() then
  dset(0,1)
card_age=0
  arc_begin_card("win",84)
 elseif deadlocked() then
  card_age=0
  arc_begin_card("fail",32767)
 end
end

function _update60()
 arc_tick()
 if arc_screen=="title" then
  if arc_pressed(4) then reset_room() end
 elseif arc_screen=="play" then
  if arc_pressed(5) then arc_screen="title" return end
  if arc_pressed(0) then move_player(-1,0,3)
  elseif arc_pressed(1) then move_player(1,0,1)
  elseif arc_pressed(2) then move_player(0,-1,0)
  elseif arc_pressed(3) then move_player(0,1,2) end
  -- hidden capture helpers: hold a + left/right
  if btn(4) and arc_pressed(1) then
   crates={{6,1},{6,5}} card_age=0 arc_begin_card("win",32767)
  elseif btn(4) and arc_pressed(0) then
   card_age=0 arc_begin_card("fail",32767)
  end
 else
  card_age+=1
  if arc_card_kind=="fail" then
   if arc_pressed(4) then reset_room()
   elseif arc_pressed(5) then arc_screen="title" end
  elseif arc_update_card() then
   px=2 py=2 crates={} moves=0 pushes=0
  end
 end
end

function diamond(cx,cy,r,fill,edge)
 local hh=max(2,flr(r/2))
 for yy=-hh,hh do
  local span=flr(r*(1-abs(yy)/(hh+1)))
  line(cx-span,cy+yy,cx+span,cy+yy,fill)
 end
 line(cx-r,cy,cx,cy-hh,edge)
 line(cx,cy-hh,cx+r,cy,edge)
 line(cx-r,cy,cx,cy+hh,edge)
 line(cx,cy+hh,cx+r,cy,edge)
end

function prism(cx,cy,r,h,top,left,right,edge)
 local hh=max(2,flr(r/2))
 -- vertical faces are deliberately tall so objects read above the board
 for lift=0,h-1 do
  line(cx-r,cy-lift,cx,cy+hh-lift,left)
  line(cx,cy+hh-lift,cx+r,cy-lift,right)
 end
 diamond(cx,cy-h,r,top,edge)
 line(cx-r,cy-h,cx-r,cy,edge)
 line(cx+r,cy-h,cx+r,cy,edge)
 line(cx,cy-h+hh,cx,cy+hh,edge)
end

function iso(x,y)
 return 64+(x-y)*7,28+(x+y)*4
end

function draw_workshop_backdrop()
 cls(1)
 rectfill(0,17,127,127,2)
 for y=20,127,9 do
  local c=(y/9)%2<1 and 2 or 5
  line(0,y,127,y,c)
 end
 for i=0,22 do
  local x=(i*37+11)%128 local y=(i*19+arc_ticks/18)%108+19
  pset(x,y,i%3==0 and 12 or 1)
 end
end

function draw_title()
 draw_workshop_backdrop()
 arc_panel(5,3,118,29,6,12)
 arc_center_text("BLOCKS &",8,7)
 arc_center_text("BUTTONS",19,10)
 -- high-angle toy diorama using the same game primitives
 for y=0,3 do for x=0,4 do
  local cx=43+(x-y)*8 local cy=55+(x+y)*5
  diamond(cx,cy,8,(x+y)%2==0 and 13 or 6,1)
 end end
 prism(35,78,8,13,9,4,8,1)
 prism(73,68,8,13,10,9,8,1)
 draw_iso_button(83,81,false,1)
 draw_robot(52,76,1)
 arc_panel(25,108,78,15,5,6)
 arc_center_text("❎  PLAY",113,7)
end

function draw_iso_floor(cx,cy,x,y)
 local color=(x+y)%2==0 and 13 or 6
 diamond(cx,cy,7,color,1)
 local h=(x*17+y*29)%11
 if h==0 then pset(cx-2,cy,12) pset(cx+2,cy-1,12) end
end

function draw_iso_wall(cx,cy)
 prism(cx,cy,7,7,3,1,5,0)
 pset(cx-2,cy-8,11) pset(cx+2,cy-6,11)
end

function draw_iso_button(cx,cy,down,pulse)
 local lift=down and 1 or 3
 prism(cx,cy,5,lift,down and 10 or 9,8,4,1)
 if not down then
  pset(cx,cy-lift-1,10)
  if pulse and sin(arc_ticks/30)>.3 then pset(cx-2,cy-lift,7) end
 end
end

function draw_iso_crate(cx,cy,solved)
 local top=solved and 10 or 9
 local left=solved and 9 or 4
 prism(cx,cy,6,10,top,left,solved and 8 or 2,1)
 line(cx-3,cy-12,cx+3,cy-8,solved and 7 or 10)
 line(cx+3,cy-12,cx-3,cy-8,solved and 7 or 10)
end

function draw_robot(cx,cy,scale)
 scale=scale or 1
 local bob=flr(sin(arc_ticks/12))
 cy+=bob
 local w=5*scale local body_h=7*scale
 -- feet stay planted while the torso and head project upward
 rectfill(cx-w,cy-3*scale,cx-1,cy,5)
 rectfill(cx+1,cy-3*scale,cx+w,cy,5)
 rectfill(cx-w,cy-body_h-3*scale,cx+w,cy-3*scale,12)
 rect(cx-w,cy-body_h-3*scale,cx+w,cy-3*scale,1)
 local hy=cy-body_h-9*scale
 rectfill(cx-w-1,hy,cx+w+1,hy+6*scale,7)
 rect(cx-w-1,hy,cx+w+1,hy+6*scale,1)
 circfill(cx-w-1,hy+2*scale,scale,10)
 circfill(cx+w+1,hy+2*scale,scale,10)
 local eye_y=hy+2*scale
 if facing==3 then pset(cx-3*scale,eye_y,0) pset(cx,eye_y,0)
 elseif facing==1 then pset(cx,eye_y,0) pset(cx+3*scale,eye_y,0)
 else pset(cx-2*scale,eye_y,0) pset(cx+2*scale,eye_y,0) end
 line(cx-2*scale,hy+5*scale,cx+2*scale,hy+5*scale,8)
end

function draw_play()
 draw_workshop_backdrop()
 print("moves:"..moves,3,6,7) print("pushes:"..pushes,78,6,10)
 -- floor first, then depth-sorted walls, buttons, crates and robot
 for y=0,bh-1 do for x=0,bw-1 do
  local cx,cy=iso(x,y)
  draw_iso_floor(cx,cy,x,y)
 end end
 for depth=0,bw+bh-2 do
  for y=0,bh-1 do
   local x=depth-y
   if x>=0 and x<bw then
    local cx,cy=iso(x,y)
    if wall(x,y) then draw_iso_wall(cx,cy) end
    if target_at(x,y) then draw_iso_button(cx,cy,crate_at(x,y)>0,1) end
    local ci=crate_at(x,y)
    if ci>0 then draw_iso_crate(cx,cy,target_at(x,y)) end
    if px==x and py==y then draw_robot(cx,cy,1) end
   end
  end
 end
end

function draw_card()
 draw_play()
 rectfill(0,0,127,127,1)
 local s=max(.05,arc_pop_scale(card_age))
 local w=flr(104*s) local h=flr(78*s)
 local x=64-flr(w/2) local y=64-flr(h/2)
 arc_panel(x,y,w,h,arc_card_kind=="win" and 13 or 2,arc_card_kind=="win" and 10 or 8)
 if s>.72 then
  if arc_card_kind=="win" then
   -- large high-angle solved crate with confetti
   prism(64,69,14,23,10,9,8,1)
   line(57,42,71,50,7) line(71,42,57,50,7)
   for i=0,9 do
    local a=i/10 local r=26+sin((arc_ticks+i)/9)*4
    pset(64+cos(a)*r,53+sin(a)*r,(i%4)+8)
   end
   arc_center_text("ALL PRESSED!",82,1)
   arc_center_text("great job!",94,7)
  else
   arc_center_text("BLOCKED!",48,8)
   arc_center_text("no crate can move",65,7)
   arc_center_text("❎ retry   🅾️ back",88,6)
  end
 end
end

function _draw()
 if arc_screen=="title" then draw_title()
 elseif arc_screen=="play" then draw_play()
 else draw_card() end
end

if qa_capture=="gameplay" then reset_room()
elseif qa_capture=="win" then
 reset_room() crates={{6,1},{6,5}} card_age=18 arc_begin_card("win",32767)
elseif qa_capture=="fail" then
 reset_room() crates={{1,1},{4,4}} card_age=18 arc_begin_card("fail",32767)
end

__gfx__
00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
