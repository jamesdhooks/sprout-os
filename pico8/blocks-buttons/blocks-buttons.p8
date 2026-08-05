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

function draw_clouds()
 cls(1)
 for i=0,7 do
  local x=(i*23+arc_ticks/8)%150-12
  local y=12+(i%3)*7
  circfill(x,y,6,12) circfill(x+6,y+1,5,12)
 end
end

function draw_title()
 draw_clouds()
 -- floating workshop island
 ovalfill(11,42,116,104,5)
 ovalfill(15,38,112,94,13)
 rectfill(25,51,102,88,6)
 for x=27,99,12 do for y=53,85,12 do rectfill(x,y,x+9,y+9,12) end end
 -- friendly block keeper
 rectfill(31,62,46,79,7) rect(31,62,46,79,10)
 circfill(35,68,2,0) circfill(42,68,2,0)
 line(35,75,42,75,8)
 -- crates and glowing buttons
 draw_crate(65,61,false,1) draw_crate(81,73,true,1)
 circfill(95,60,6,9) circfill(95,60,3,10)
 arc_panel(7,4,114,32,6,12)
 arc_center_text("BLOCKS &",10,7)
 arc_center_text("BUTTONS",21,10)
 arc_panel(25,108,78,15,5,6)
 arc_center_text("❎  PLAY",113,7)
end

function draw_floor(x,y)
 rectfill(x,y,x+cs-1,y+cs-1,13)
 if ((x+y)/cs)%2<1 then pset(x+2,y+3,6) pset(x+8,y+9,6) end
end

function draw_wall(x,y)
 rectfill(x,y,x+cs-1,y+cs-1,5)
 rectfill(x,y,x+cs-1,y+2,6)
 line(x+2,y+5,x+9,y+5,1)
 line(x+1,y+10,x+7,y+10,1)
end

function draw_button(x,y,down)
 local c=down and 11 or 9
 circfill(x+6,y+7,5,2) circfill(x+6,y+5,4,c)
 if not down then circfill(x+5,y+4,1,10) end
end

function draw_crate(x,y,solved,scale)
 scale=scale or 1
 local s=10*scale
 rectfill(x,y,x+s,y+s,solved and 10 or 9)
 rect(x,y,x+s,y+s,solved and 7 or 4)
 line(x+2*scale,y+2*scale,x+s-2*scale,y+s-2*scale,solved and 7 or 4)
 line(x+s-2*scale,y+2*scale,x+2*scale,y+s-2*scale,solved and 7 or 4)
end

function draw_keeper(x,y)
 local bob=flr(sin(arc_ticks/8)*1.2)
 y+=bob
 circfill(x+6,y+6,5,7)
 rectfill(x+2,y+6,x+10,y+10,7)
 pset(x+4,y+5,0) pset(x+8,y+5,0)
 if facing==0 then line(x+4,y+1,x+4,y-1,10)
 elseif facing==2 then line(x+7,y+11,x+7,y+13,10)
 elseif facing==1 then line(x+11,y+5,x+13,y+5,10)
 else line(x+1,y+5,x-1,y+5,10) end
end

function draw_play()
 cls(1)
 rectfill(0,0,127,19,1)
 print("moves:"..moves,4,6,7) print("pushes:"..pushes,80,6,10)
 for y=0,bh-1 do for x=0,bw-1 do
  local sx=bx+x*cs local sy=by+y*cs
  if wall(x,y) then draw_wall(sx,sy) else draw_floor(sx,sy) end
 end end
 for t in all(targets) do
  draw_button(bx+t[1]*cs,by+t[2]*cs,crate_at(t[1],t[2])>0)
 end
 for c in all(crates) do
  draw_crate(bx+c[1]*cs+1,by+c[2]*cs+1,target_at(c[1],c[2]))
 end
 draw_keeper(bx+px*cs,by+py*cs)
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
   -- large solved crate with confetti
   draw_crate(49,38,true,3)
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
