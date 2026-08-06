#!/usr/bin/env python3
"""Build the Mouse & Cheese PICO-8 gameplay atlas and title-screen payload."""
from __future__ import annotations

import argparse
import json
import tempfile
import shutil
from pathlib import Path
from PIL import Image, ImageDraw

PALETTE=((0,0,0),(29,43,83),(126,37,83),(0,135,81),(171,82,54),(95,87,79),(194,195,199),(255,241,232),(255,0,77),(255,163,0),(255,236,39),(0,228,54),(41,173,255),(131,118,156),(255,119,168),(255,204,170))
MOUSE_ALLOWED=set("01346789abcef")
MOUSE_REMAP={"2":"e","5":"4","d":"6"}
TITLE_BEGIN="-- BEGIN GENERATED TITLE"
TITLE_END="-- END GENERATED TITLE"

def color(rgb):
 return format(min(range(16),key=lambda i:sum((rgb[n]-PALETTE[i][n])**2 for n in range(3))),"x")

def stamp(image,pixels,x,y):
 for yy in range(image.height):
  for xx in range(image.width):
   pixel=image.getpixel((xx,yy))
   if len(pixel)==4 and pixel[3]==0: continue
   value=color(pixel[:3])
   value=MOUSE_REMAP.get(value,value)
   # Palette 0 is the transparent key. Preserve reference-black facial and
   # outline pixels as PICO navy rather than accidentally cutting them out.
   pixels[y+yy][x+xx]="1" if value=="0" else value

def running_pose(frame,direction):
 pose=frame.copy()
 # Preserve the face, body and tail exactly. Only pink/shadow pixels in the
 # two lower foot zones move, producing a tiny alternating stride rather than
 # deforming the mouse's lower silhouette like a mouth opening.
 if direction==1:
  for left,right,shift,target in ((0,.42,-1,.32),(.58,1,1,.68)):
   candidates=[(x,y,frame.getpixel((x,y)))
               for y in range(frame.height) for x in range(frame.width)
               if (y>=frame.height*.45 and left*frame.width<=x<right*frame.width
                   and frame.getpixel((x,y))[3])]
   if candidates:
    x,y,pixel=max(candidates,key=lambda item:(item[1],-abs(item[0]-target*frame.width)))
    pose.putpixel((max(0,min(frame.width-1,x+shift)),y),pixel)
  return pose
 if direction==0:
  zones=((0,.42,-1),(.58,1,1))
 else:
  zones=((.25,.5,-1),(.5,.75,1))
 for left,right,shift in zones:
  foot=[]
  for y in range(frame.height):
   for x in range(frame.width):
    r,g,b,a=frame.getpixel((x,y))
    if (a and y>=frame.height*.68 and left*frame.width<=x<right*frame.width
        and r-g>20 and r-b>10):
     foot.append((x,y,(r,g,b,a)))
  for x,y,_ in foot: pose.putpixel((x,y),(0,0,0,0))
  for x,y,pixel in foot:
   pose.putpixel((max(0,min(frame.width-1,x+shift)),y),pixel)
  if not foot:
   candidates=[(x,y,frame.getpixel((x,y)))
               for y in range(frame.height) for x in range(frame.width)
               if (y>=frame.height*.55 and left*frame.width<=x<right*frame.width
                   and frame.getpixel((x,y))[3])]
   if candidates:
    target=(left+right)*frame.width/2
    x,y,pixel=max(candidates,key=lambda item:(item[1],-abs(item[0]-target)))
    nx=max(0,min(frame.width-1,x+shift)); ny=y
    if nx==x or frame.getpixel((nx,ny))[3]: nx=x; ny=min(frame.height-1,y+1)
    if nx!=x or ny!=y:
     pose.putpixel((x,y),(0,0,0,0)); pose.putpixel((nx,ny),pixel)
 return pose

CHEESE_PROFILES={
 9:["000000000","000004400","0004aa400","004aaa940","04aa99940","4a9999940","499949940","499999940","044444400"],
 8:["00000000","00004400","004aa400","04aaa940","4aa99940","49994940","49999940","04444440"],
 7:["0000000","0004400","004a400","04aa940","4a99940","4994940","0444440"],
 6:["000000","000440","004a40","04a940","499940","044440"],
 5:["00400","04f40","4aa40","4a940","04440"],
}

def cheese_profile(size):
 image=Image.new("RGBA",(size,size),(0,0,0,0))
 for y,row in enumerate(CHEESE_PROFILES[size]):
  for x,value in enumerate(row):
   if value!="0": image.putpixel((x,y),(*PALETTE[int(value,16)],255))
 return image

MOUSE_9={
 0:["000110000","001ee1000","011771100","017777710","017177710","017777710","001771000","000e0e000","0000e0000"],
 1:["0000e0000","000e0e000","001771000","017777710","017177710","017777710","011771100","001ee1000","000110000"],
 2:["000110000","001ee1000","011777100","017717710","e17777710","017777100","001771000","00e0e0000","0000eee00"],
 3:["000011000","0001ee100","001777110","017717710","01777771e","001777710","000177100","0000e0e00","00eee0000"],
}

def indexed_sprite(rows):
 image=Image.new("RGBA",(len(rows[0]),len(rows)),(0,0,0,0))
 for y,row in enumerate(rows):
  for x,value in enumerate(row):
   if value!="0": image.putpixel((x,y),(*PALETTE[int(value,16)],255))
 return image

def authored_mouse(size,direction,stride=0):
 if size==9:
  image=indexed_sprite(MOUSE_9[direction])
  # The alternate run frame moves only the two feet, never the face/body.
  if stride:
   pixels=image.load()
   for x in range(9):
    if pixels[x,7][:3]==PALETTE[14]: pixels[x,7]=(0,0,0,0)
   if direction in (0,1):
    image.putpixel((2,7),(*PALETTE[14],255)); image.putpixel((6,7),(*PALETTE[14],255))
  return image
 image=Image.new("RGBA",(size,size),(0,0,0,0)); draw=ImageDraw.Draw(image)
 outline=(*PALETTE[1],255); white=(*PALETTE[7],255); shade=(*PALETTE[6],255); pink=(*PALETTE[14],255)
 if direction in (0,1):
  # South/north: a compact pear body with separated ears and tail.
  draw.ellipse((2,2,size-3,size-2),fill=outline)
  draw.ellipse((3,3,size-4,size-3),fill=white)
  ear_y=1 if direction==0 else size-4
  draw.ellipse((1,ear_y,4,ear_y+3),fill=outline); draw.ellipse((size-5,ear_y,size-2,ear_y+3),fill=outline)
  draw.point((2,ear_y+1),fill=pink); draw.point((size-3,ear_y+1),fill=pink)
  face_y=4 if direction==0 else size-5
  draw.point((4,face_y),fill=outline); draw.point((size-5,face_y),fill=outline)
  tail_y=size-1 if direction==0 else 0
  draw.line((size//2, size-3 if direction==0 else 2, size//2+2,tail_y),fill=pink)
  foot_y=size-2 if direction==0 else 1
  draw.point((3+stride,foot_y),fill=pink); draw.point((size-4-stride,foot_y),fill=pink)
 else:
  # West/east are one deliberate side silhouette; east is not inferred from title art.
  left=direction==2
  draw.ellipse((2,3,size-3,size-3),fill=outline); draw.ellipse((3,4,size-4,size-4),fill=white)
  nose_x=1 if left else size-2
  draw.point((nose_x,size//2),fill=pink)
  ear_x=4 if left else size-5
  draw.ellipse((ear_x-1,2,ear_x+2,5),fill=outline); draw.point((ear_x,3),fill=pink)
  eye_x=3 if left else size-4; draw.point((eye_x,5),fill=outline)
  tail_start=size-3 if left else 2; tail_end=size-1 if left else 0
  draw.line((tail_start,size-4,tail_end,size-2),fill=pink)
  draw.point((4+stride,size-2),fill=pink); draw.point((size-5-stride,size-2),fill=pink)
 # one stable shadow pixel gives the body volume without changing its silhouette
 draw.point((size//2,size-3),fill=shade)
 return image

def title_payload(image):
 values="".join(color(image.getpixel((x,y))) for y in range(128) for x in range(128))
 out=[]; at=0
 while at<len(values):
  end=at+1
  while end<len(values) and values[end]==values[at] and end-at<15: end+=1
  out.append(format(end-at,"x")+values[at]); at=end
 return "".join(out)

def chunks(value):
 return '"'+value[:96]+'"'+''.join('\n.."'+value[i:i+96]+'"' for i in range(96,len(value),96))

def chroma_frames(reference):
 image=Image.open(reference).convert("RGB")
 # The reference is deliberately generated on magenta. Segment subjects by
 # columns, preserving antialiased subject colours while making the backdrop
 # PICO transparent colour 0.
 foreground=[]
 for x in range(image.width):
  foreground.append(any(not (r>180 and g<90 and b>120) for r,g,b in (image.getpixel((x,y)) for y in range(image.height))))
 spans=[]; start=None
 for x,occupied in enumerate(foreground+[False]):
  if occupied and start is None: start=x
  if not occupied and start is not None:
   if x-start>30: spans.append((start,x))
   start=None
 if len(spans)!=5: raise ValueError(f"expected 5 sprite-reference subjects, found {len(spans)}")
 frames=[]
 for start,end in spans:
  points=[(x,y) for x in range(start,end) for y in range(image.height) if not (lambda p:p[0]>180 and p[1]<90 and p[2]>120)(image.getpixel((x,y)))]
  left=min(x for x,_ in points); right=max(x for x,_ in points)+1
  top=min(y for _,y in points); bottom=max(y for _,y in points)+1
  crop=image.crop((left,top,right,bottom))
  out=Image.new("RGBA",crop.size,(0,0,0,0))
  for y in range(crop.height):
   for x in range(crop.width):
    r,g,b=crop.getpixel((x,y))
    if not (r>180 and g<90 and b>120): out.putpixel((x,y),(r,g,b,255))
  frames.append(out)
 return frames

def build(sprites,title,preview,reference=None):
 payload=json.loads(sprites.read_text(encoding="utf-8")); pixels=[["0"]*128 for _ in range(128)]
 source=Image.open(title).convert("RGB").resize((128,128),Image.Resampling.LANCZOS)
 cheese=source.crop((74,52,102,80))
 # Exact-size sprite families; all transparent pixels remain palette 0.
 alternate_y={12:80,9:96}
 cheese_sizes={12:9,9:8}
 for size,y in ((12,0),(9,16)):
  for direction in range(4):
   frame=authored_mouse(size,direction,0)
   stamp(frame,pixels,direction*(size+2),y)
   stamp(authored_mouse(size,direction,1),pixels,direction*(size+2),alternate_y[size])
  stamp(cheese_profile(cheese_sizes[size]),pixels,64,y)
 # The victory card uses its own nearly native-size cheese illustration rather
 # than scaling the 14px gameplay pickup into a blocky overlay.
 celebration=cheese.convert("RGBA"); celebration.thumbnail((28,28),Image.Resampling.NEAREST)
 canvas=Image.new("RGBA",(28,28),(0,0,0,0)); canvas.alpha_composite(celebration,((28-celebration.width)//2,(28-celebration.height)//2))
 stamp(canvas,pixels,80,0)
 # Two-pixel-scale victory lettering is stored as atlas art so it stays crisp
 # and substantially larger than PICO-8's built-in 4x6 font.
 glyphs={
  "c":["01110","10000","10000","10000","10000","10000","01110"],
  "h":["10001","10001","10001","11111","10001","10001","10001"],
  "e":["11111","10000","10000","11110","10000","10000","11111"],
  "s":["01111","10000","10000","01110","00001","00001","11110"],
  "!":["00100","00100","00100","00100","00100","00000","00100"],
 }
 label="cheese!"; label_x,label_y=1,65
 lit=[]
 for index,letter in enumerate(label):
  for gy,row in enumerate(glyphs[letter]):
   for gx,value in enumerate(row):
    if value=="1":
     for yy in range(2):
      for xx in range(2): lit.append((label_x+index*13+gx*2+xx,label_y+gy*2+yy))
 for x,y in lit:
  for oy in (-1,0,1):
   for ox in (-1,0,1):
    if 0<=x+ox<128 and 0<=y+oy<128: pixels[y+oy][x+ox]="9"
 for x,y in lit: pixels[y][x]="f"
 # Stable 8x8 tiles live below the character families; source keys remain local IDs.
 for key,rows in payload["sprites"].items():
  index=96+int(key); ox,oy=(index%16)*8,(index//16)*8
  for yy,row in enumerate(rows):
   pixels[oy+yy][ox:ox+8]=[MOUSE_REMAP.get(value,value) for value in row]
 if preview:
  rendered=Image.new("RGB",(128,128)); rendered.putdata([PALETTE[int(v,16)] for row in pixels for v in row]); rendered.save(preview)
 return "\n".join("".join(row) for row in pixels),title_payload(source)

def inject(cart,gfx,title):
 text=cart.read_text(encoding="utf-8"); head,tail=text.split("__gfx__\n",1); _old,sections=tail.split("__gff__",1)
 start=text.find(TITLE_BEGIN); end=text.find(TITLE_END,start)
 if start<0 or end<start: raise ValueError("cart lacks title payload markers")
 generated=TITLE_BEGIN+'\ntitle_data='+chunks(title)+'\n'+TITLE_END
 text=head+"__gfx__\n"+gfx+"\n__gff__"+sections
 start=text.find(TITLE_BEGIN); end=text.find(TITLE_END,start)+len(TITLE_END)
 cart.write_text(text[:start]+generated+text[end:],encoding="utf-8",newline="\n")

def main():
 p=argparse.ArgumentParser(); p.add_argument("--sprites",type=Path,required=True); p.add_argument("--title-source",type=Path,required=True); p.add_argument("--sprite-reference",type=Path); p.add_argument("--cart",type=Path,required=True); p.add_argument("--preview",type=Path); p.add_argument("--check",action="store_true"); a=p.parse_args()
 if a.check:
  with tempfile.TemporaryDirectory() as directory:
   temporary=Path(directory)/a.cart.name; shutil.copy2(a.cart,temporary)
   temporary_preview=Path(directory)/"preview.png" if a.preview else None
   gfx,title=build(a.sprites,a.title_source,temporary_preview,a.sprite_reference); inject(temporary,gfx,title)
   stale=temporary.read_bytes()!=a.cart.read_bytes()
   if a.preview: stale=stale or not a.preview.exists() or temporary_preview.read_bytes()!=a.preview.read_bytes()
   if stale: raise SystemExit("Mouse & Cheese PICO assets are stale")
 else:
  gfx,title=build(a.sprites,a.title_source,a.preview,a.sprite_reference); inject(a.cart,gfx,title)
if __name__=="__main__": main()
