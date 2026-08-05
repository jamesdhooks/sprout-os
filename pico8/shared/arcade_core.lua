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
