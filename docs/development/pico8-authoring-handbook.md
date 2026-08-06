# PICO-8 authoring handbook

This is a compact working manual for an experienced game developer who is new
to PICO-8. It focuses on the machine model, authoring loop, APIs, constraints,
and traps that materially affect game design. It is not a Lua or game-design
tutorial.

Use this document to learn PICO-8. Use the companion
[Sprout PICO-8 workflow](pico8-game-development.md) to scaffold, validate,
capture, and deploy a finished cart.

## The machine in one page

PICO-8 is a deliberately constrained fantasy console. A cartridge contains
code, graphics, map data, sound effects, and music. The same cart runs in the
editor, the desktop runtime, exported players, and compatible interpreters such
as fake-08.

| Resource | Practical limit |
| --- | --- |
| Display | 128x128 pixels |
| Base palette | 16 colours, with runtime remapping |
| Input | D-pad plus two action buttons per player |
| Code | 8,192 P8 Lua tokens |
| Cartridge | 32 KB encoded data |
| Compressed code in `.p8.png`/`.p8.rom` | Less than 15,360 bytes |
| CPU | 4 million virtual-machine instructions per second |
| Sprites | 128 dedicated 8x8 sprites, plus 128 sharing map memory |
| Map | 128x32 tiles, or 128x64 when using shared sprite memory |
| Audio | Four channels, 64 SFX, 64 music patterns |
| Persistent data | 64 numbers after selecting a `cartdata()` ID |

Coordinates use a top-left origin. Positive X points right and positive Y points
down. A nominal sprite is 8x8, but `spr()` can draw rectangular groups and
`sspr()` can address arbitrary source rectangles.

The constraints are coupled. Using sprites 128-255 consumes the same cartridge
space as map rows 32-63. Large source tables consume code tokens and compressed
bytes. A 60 Hz game has half the per-frame CPU budget of a 30 Hz game.

### Memory map when you need direct access

Most games should begin with high-level APIs. `peek()`, `poke()`, `memcpy()`,
and `memset()` become useful for generated graphics, bulk map operations, custom
data packing, and profiling experiments.

| Address | Contents |
| --- | --- |
| `0x0000` | First half of sprite graphics |
| `0x1000` | Shared second-half graphics / lower map |
| `0x2000` | Upper map |
| `0x3000` | Sprite flags |
| `0x3100` | Music patterns |
| `0x3200` | SFX data |
| `0x4300` | General user RAM |
| `0x5e00` | 256-byte persistent cart-data window |
| `0x5f00` | Draw state and special registers |
| `0x5f40` | Hardware state |
| `0x6000` | 8 KB screen framebuffer |

Graphics and screen memory pack two four-bit pixels into each byte. Map cells
are one byte each. Do not adopt hard-coded register pokes from an old cart
without checking the current official manual and the fake-08 target.

## The authoring loop

Start PICO-8 and you arrive at its console. The core commands are:

```text
load mygame       load mygame.p8
save mygame       save the working cart
run               run the loaded cart
resume            continue a stopped cart when possible
info              show tokens, characters, and compressed size
folder            open the current cart directory in Windows Explorer
backup            make an explicit backup
ls                list the virtual directory
cd games          change virtual directory
mkdir prototypes  create a directory
install_demos     install the official example carts
```

Useful global keys:

| Key | Effect |
| --- | --- |
| `Esc` | Stop a running cart; toggle console/editor when stopped |
| `Ctrl+R` | Reload and run the working cartridge |
| `Ctrl+S` | Quick-save |
| `Alt+Left/Right` | Cycle editor modes |
| `Alt+Enter` | Toggle fullscreen |
| `Enter` or `P` | Open the runtime pause menu |
| `Ctrl+6` | Save a screenshot |
| `Ctrl+7` | Capture the current screen as the cart label |
| `Ctrl+8`, `Ctrl+9` | Start GIF range, then save GIF |

For an external-editor workflow:

1. Keep the canonical `.p8` file in the repository.
2. Open it in PICO-8 through `python tools/pico8_game.py run <slug>` or drag it
   onto the PICO-8 window.
3. Edit the text cart externally while PICO-8 has no unsaved editor changes.
4. Press `Ctrl+R`; PICO-8 detects the changed file, reloads, and runs it.
5. Edit sprite/map/audio data inside PICO-8, then `Ctrl+S` before returning to
   the external editor.

If both the external file and PICO-8's in-memory cart changed, PICO-8 refuses to
reload rather than choosing a winner. Save or discard one side deliberately.

The default Windows PICO drive is:

```text
C:\Users\<name>\AppData\Roaming\pico-8\carts
```

Repository carts do not need to live there when loaded by full path or drag and
drop.

## Editor modes

### Code editor

Code tabs are organizational only. At runtime, PICO-8 concatenates every tab
from left to right into one program. Tabs do not create modules or scopes.

High-value shortcuts:

- `Ctrl+F`: search the current tab.
- `Ctrl+L`: jump to a line.
- `Alt+Up/Down`: previous or next function.
- `Ctrl+Tab`: change code tab.
- `Ctrl+B`: comment or uncomment a selection.
- `Ctrl+U`: open help for the symbol under the cursor.
- Right-click the lower-right code statistic to cycle token, character, and
  compressed-size displays.

### Sprite editor

The sheet is a 128x128 image viewed as 256 8x8 cells. Sprite index `n` maps to:

```lua
sx=(n%16)*8
sy=flr(n/16)*8
```

Useful operations include rectangular selection, copy/paste, horizontal flip
`F`, vertical flip `V`, rotate `R`, and arrow-key shifting. Sprite flags are the
eight coloured toggles associated with each sprite and are read with `fget()`.

Colour 0 is transparent for sprite drawing by default. That is a draw rule, not
alpha stored in the sprite sheet. Use `palt()` when a different transparency
rule is required, and reset it after the special draw.

### Map editor

The map stores bytes. The editor interprets each byte as a sprite index, but
game code can treat it as any compact 2D data structure.

Rows 0-31 are always available. Rows 32-63 overlap sprites 128-255. Decide early
whether the game is art-heavy or map-heavy and document the allocation.

A common tile convention is:

| Sprite flag | Meaning |
| --- | --- |
| 0 | Solid collision |
| 1 | Hazard |
| 2 | Interactive |
| 3 | Foreground/overdraw |

Flags have no built-in semantics; this table is only a game-owned contract.

### SFX editor

Each of the 64 SFX contains 32 notes. Each note has pitch, waveform/instrument,
volume, and effect. SFX also has playback speed and loop bounds.

Use pitch mode for gestures such as jump, impact, pickup, and UI confirmation.
Use tracker mode for pitched phrases and reusable music material. Reserve a
small range by convention, for example:

```text
00-15 gameplay SFX
16-23 UI SFX
24-31 ambient loops
32-63 music phrases
```

### Music editor

A music pattern schedules up to four SFX, one per audio channel. Patterns can
loop or chain. Music does not have separate note storage; it sequences the same
SFX bank used by sound effects.

If music reserves all channels, opportunistic `sfx(n)` calls may interrupt or
be starved by music. Deliberately budget channels, or request an explicit
channel for high-priority feedback.

## Program lifecycle

The conventional entry points are:

```lua
function _init()
 -- called once when the cart starts
end

function _update60()
 -- fixed simulation update, 60 times per second when budget permits
end

function _draw()
 -- render the current frame
end
```

Define `_update()` instead of `_update60()` for 30 Hz. Do not define both as a
way to negotiate dynamically. If the renderer misses budget, PICO-8 can call
multiple updates for one visible draw, so simulation belongs in update and
rendering belongs in draw.

For most small games, use explicit screen state instead of a framework:

```lua
screen="title"

function _update60()
 if screen=="title" then update_title()
 elseif screen=="play" then update_play()
 elseif screen=="win" then update_win()
 end
end

function _draw()
 if screen=="title" then draw_title()
 elseif screen=="play" then draw_play()
 else draw_win() end
end
```

Keep state transitions in small functions. Initialize the destination state in
the same transition so stale state cannot leak between rounds.

## P8 Lua: differences that matter

PICO-8 uses its own Lua dialect and API, not the normal Lua standard library.

### Numbers

Normal numbers are signed 16:16 fixed point, approximately -32768 through
32767.99998. Overflow and limited precision are real game constraints.

```lua
x=12.5
x+=0.25       -- PICO shorthand assignment
whole=flr(x)
packed=0x12.abcd
```

Angles use turns, not radians: `0.25` is a quarter turn. PICO-8's `sin()` is
inverted for screen-space Y, so `sin(0.25)` is `-1`. Verify rotation direction
instead of pasting conventional radian code unchanged.

### Tables

Tables are both arrays and dictionaries. Array-style literals begin at index 1.

```lua
actors={}
add(actors,{x=20,y=30,hp=3})

for actor in all(actors) do
 actor.x+=1
end

del(actors,actor)  -- delete by value
deli(actors,2)     -- delete by index
```

Use `pairs()` for dictionaries, but never depend on its traversal order. Avoid
allocating temporary tables in inner update/draw loops unless profiling shows
the cost is irrelevant.

### Truth, equality, and syntax

Only `false` and `nil` are false; `0` and empty strings are true. Arrays are
one-based unless you explicitly define index 0. PICO-8 accepts both `~=` and
`!=`, plus shorthand operators such as `+=`, `-=`, and `..=`.

PICO-8 also supports one-line shorthand forms, but conventional Lua blocks are
easier to debug and port. Optimize syntax for tokens only after the game works.

### Randomness

`rnd(x)` returns `[0,x)` for a number and a random value for an array-style
table. Use `flr(rnd(n))` for an integer in `[0,n-1]`. `srand(seed)` makes a
sequence reproducible.

Do not reseed inside `_draw()`. Generate level/gameplay state during a state
transition, store it, and render that stable result.

### Time

`time()`/`t()` advances from update calls; it is not wall-clock time. Prefer an
integer tick counter for deterministic gameplay and use `time()` for cosmetic
animation when exact replay is unnecessary.

## Input

Buttons are:

```text
0 left   1 right   2 up   3 down   4 O/A   5 X/B
```

Desktop player-one defaults include arrows plus `Z` for button 4 and `X` for
button 5.

```lua
if btn(0) then x-=speed end     -- held
if btnp(4) then jump() end      -- newly pressed, then key-repeat
```

`btnp()` is not a pure edge detector: after its initial press it repeats after
a delay. That is useful for menus and grid movement but can surprise action
code. For an exact edge, retain the prior `btn()` bitfield yourself:

```lua
function sample_input()
 local now=btn()
 pressed=now&~previous
 previous=now
end
```

Physical controllers cannot reliably express contradictory D-pad directions,
and some diagonal-plus-action chords are awkward. Design around the two action
buttons rather than keyboard-only combinations.

Mouse and full keyboard input require devkit mode and are unsuitable as a
mandatory control path for Sprout/Miyoo games.

## Drawing

The important APIs are small:

```lua
cls(c)
pset(x,y,c)
line(x0,y0,x1,y1,c)
rect(x0,y0,x1,y1,c)
rectfill(x0,y0,x1,y1,c)
circ(x,y,r,c)
circfill(x,y,r,c)
print(text,x,y,c)
spr(id,x,y,w,h,flip_x,flip_y)
sspr(sx,sy,sw,sh,dx,dy,dw,dh,flip_x,flip_y)
map(mx,my,sx,sy,mw,mh,layer_mask)
```

`spr()` is ideal for grid-aligned sprites and multi-cell sprite blocks. `sspr()`
is the general blitter: arbitrary source rectangles, destination scaling, and
flipping. Scaling does not create detail; for readable tiered art, author
separate sprite families.

### Draw state

Graphics calls share mutable state:

- `camera(x,y)` offsets subsequent drawing by `-x,-y`.
- `clip(x,y,w,h)` constrains drawing.
- `pal(a,b)` remaps colour `a` to `b` for later draws.
- `palt(c,true)` marks a sprite colour transparent.
- `fillp(pattern)` changes primitive fill patterns.

Reset temporary state explicitly:

```lua
camera()
clip()
pal()
palt()
fillp()
```

Do that at the start of each major render pass or immediately after a local
effect. Leaked camera/palette/clip state causes many apparently inexplicable
rendering bugs.

### Palette strategy

The base palette is coherent but small. Prefer role-based allocation over using
every colour equally:

- one or two background families;
- one readable foreground/actor family;
- a high-contrast UI pair;
- one danger/accent family;
- reserved highlight and shadow colours.

Use draw-palette swaps for damage flashes, variants, focus states, and fades.
Display-palette remapping can affect the whole completed screen. Avoid changing
palette state just to save one duplicate 8x8 sprite if it makes the render path
hard to reason about.

### Camera and layering

World rendering usually follows:

```lua
camera(cam_x,cam_y)
map(0,0)
draw_world_actors()
draw_foreground_tiles()
camera()
draw_ui()
```

Use sprite flags plus the `map()` layer mask for background/foreground passes.
Do not let world camera state affect HUD coordinates.

## Tile maps and collision

The simplest robust collision system uses tile values for visuals and sprite
flags for properties:

```lua
function solid_at_pixel(x,y)
 local tile=mget(flr(x/8),flr(y/8))
 return fget(tile,0)
end
```

For an axis-aligned body, test the leading corners independently on X and Y,
resolve one axis at a time, and keep rendering interpolation separate from the
authoritative collision position.

Do not assume every map tile is 8x8 in the game design. The map is just bytes;
you may use metatiles, rooms, object markers, or compressed level descriptors.
Clear spawn markers after reading them if they should not render as terrain.

## Animation

Store animation as data, not conditionals distributed through draw code:

```lua
walk={16,17,18,17}

function actor_sprite(actor)
 if actor.moving then
  return walk[1+flr(actor.anim_tick/8)%#walk]
 end
 return 16
end
```

At 60 Hz, dividing by 8 produces 7.5 animation frames per second. Advance the
animation clock only while the action is active. Keep every frame on the same
ground-contact baseline; use explicit per-frame offsets only when the motion is
intentional.

For directional animation, author north/south and one horizontal family, then
flip horizontally only when the design is actually symmetrical.

## Audio

```lua
sfx(3)          -- choose a free channel
sfx(3,2)        -- force channel 2
sfx(-1,2)       -- stop channel 2
music(0,500)    -- start pattern 0 with a 500 ms fade
music(-1,500)   -- stop music with a fade
```

Treat channels as a mix budget:

- reserve one or two for music;
- keep one for moment-to-moment game feedback;
- give critical sounds an explicit channel;
- avoid starting the same SFX every update while a condition remains true.

Use `stat(46)` through `stat(56)` when gameplay or visualization genuinely
needs current mixer/music state.

## Saving progress

PICO-8 persistence is intentionally tiny:

```lua
function _init()
 cartdata("my_game_v1")
 local schema=dget(0)
 if schema!=1 then
  reset_save()
  dset(0,1)
 end
 best=flr(dget(1))
end

function save_best(value)
 dset(1,value)
end
```

Rules worth enforcing:

- Call `cartdata()` before `dget()`/`dset()`.
- Use only lowercase letters, digits, and underscores in Sprout save IDs.
- Reserve slot 0 for a schema version.
- Document all 64 slot assignments.
- Store compact scalar state, not a serialized object graph.
- Write on meaningful transitions, not every frame.
- On incompatible data, reset only this game's slots.

Sprout rewrites the designated development ID in a managed cart to isolate
progress by family profile. Do not compute household identity inside the cart.

## Debugging

PICO-8 has no conventional source debugger, so make state observable.

```lua
?"entered room "..room_id        -- shorthand for print at console/current cursor
printh("seed="..seed)             -- host log/file output
assert(player,"missing player")
```

Build a development overlay:

```lua
function draw_debug()
 camera()
 rectfill(0,0,127,7,0)
 print("cpu:"..flr(stat(1)*100).."%",1,1,7)
 print("x:"..flr(px).." y:"..flr(py),52,1,7)
end
```

High-value deterministic hooks include:

- start at a named screen (`title`, `gameplay`, `win`, `fail`);
- jump to a level/seed;
- force a reward, failure, or boss phase;
- render collision shapes and sprite anchors;
- freeze simulation and advance one tick;
- display CPU with `stat(1)` and framerate with `stat(7)`.

Keep release defaults normal and ensure capture/test tools modify only temporary
cart copies.

When a cart crashes, PICO-8 reports the failing line. Inspect the value types
feeding that line first; accidental `nil`, a misspelled global, and table shape
drift are common causes.

## Performance and budget discipline

Use `info` continuously, not only at release. The three separate pressures are:

- token count;
- compressed code size;
- per-frame CPU (`stat(1)`).

Practical order of optimization:

1. Stop doing work that does not affect the frame.
2. Precompute level/static render data when entering a state.
3. Bound searches and procedural generation.
4. Avoid repeated table/string allocation in inner loops.
5. Cull off-screen objects and effects.
6. Reduce expensive overdraw and large per-pixel loops.
7. Only then shorten code for token/compression budget.

For static procedural terrain, generate once per level into a compact table,
map region, or sprite/screen memory cache. Do not regenerate full-screen noise
every draw.

At 60 Hz, keep comfortable headroom below `stat(1)==1`. A desktop pass at the
edge of budget is not evidence that fake-08 will hold frame rate on Miyoo.

## Cartridge structure and source control

A text cart is divided into sections:

```text
pico-8 cartridge // http://www.pico-8.com
version 43
__lua__
...
__gfx__
...
__gff__
...
__map__
...
__sfx__
...
__music__
...
```

Commit `.p8` text carts because they are reviewable and mergeable. Use
`.p8.png` for distribution when appropriate, not as the only source artifact.

`#include` can improve desktop authoring, but a Sprout device cart must be
self-contained. Inject shared/generated code before release and validate the
resulting single cart.

Avoid mass-reformatting cartridge sections. GFX/map/audio data is dense and
unrelated changes make review unnecessarily difficult.

## Export and release

Useful formats:

- `.p8`: canonical text source.
- `.p8.png`: visual cartridge containing the encoded game.
- `.html`: standalone web player export.
- `.bin`: platform-specific standalone player export.

The console `export` command produces players; the desktop executable also
supports headless `-export`. Sprout/Onion deployment normally needs the
standalone `.p8` cart, not a desktop binary export.

### fake-08 compatibility

Treat the licensed desktop runtime as the authoring authority and fake-08 as a
separate target implementation. Stay on the conventional API path unless a
feature has been verified on-device:

- standard six-button `btn()`/`btnp()` input;
- ordinary `spr()`, `sspr()`, `map()`, primitives, palette swaps, SFX, and music;
- one self-contained cart;
- `cartdata()` plus `dget()`/`dset()` persistence;
- no required mouse, keyboard, host filesystem, clipboard, GPIO, or multi-cart
  loading;
- no assumption that a desktop-edge CPU budget will perform identically.

When behavior differs, isolate the smallest reproducing cart before changing
the game. That distinguishes an interpreter incompatibility from game logic or
asset corruption.

Before release:

1. Run `info` and verify token/compressed limits.
2. Test from a fresh boot and existing save.
3. Test every screen and terminal state.
4. Verify both action buttons and pause/exit behavior.
5. Capture a label with `Ctrl+7` if shipping `.p8.png`.
6. Test on desktop PICO-8.
7. Test on fake-08/Miyoo; do not infer compatibility from desktop.

## A sensible first manual project

Start with one screen, one controllable actor, one goal, and one terminal state:

1. Scaffold the cart.
2. Replace the title copy and palette.
3. Draw one 8x8 or 16x16 actor in the sprite editor.
4. Move it with held `btn()` input in `_update60()`.
5. Draw it with `spr()`.
6. Add one map room and flag solid tiles.
7. Add collision one axis at a time.
8. Add a pickup and one SFX.
9. Save one high score with `cartdata()`.
10. Add deterministic title/gameplay/win capture states.
11. Run the Sprout validator, then deploy over SSH.

That vertical slice exercises nearly every PICO-8 subsystem without creating a
large architecture before you understand the machine.

## Primary references

- [Official PICO-8 manual](https://www.lexaloffle.com/dl/docs/pico-8_manual.html)
- [Official PICO-8 resources](https://www.lexaloffle.com/pico-8.php?page=resources)
- [Onion PICO-8/fake-08 notes](https://onionui.github.io/docs/emulators/pico-8)
- [Sprout creation and deployment workflow](pico8-game-development.md)
