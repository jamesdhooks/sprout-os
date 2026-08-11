# Blocks & Buttons PICO-8 Edition

The canonical PICO artwork is `sprites.json`. Every frame is a literal palette
index grid: fifteen 16x16 robot frames plus floor, wall, button, crate, and
solved-crate tiles. Directional source art and motion references are reduced,
palette-locked, and hand-refined at final size before entering these grids.
East-facing robot frames mirror west at runtime.
The 8x8 board renders at an exact 16 pixels per cell and occupies the complete
128x128 display; no HUD strip reduces the playfield.

Collision and artwork cells are both 16x16. Walls and crates remain strict
orthographic squares, buttons use a compact circular silhouette, and the robot
keeps a stable bottom-center anchor during interpolated movement. The resulting
board is readable without perspective overlap or special-case object scaling.

The gameplay atlas follows the title's object language: warm wooden crates use
a simple cross brace, buttons use one red cap inside a silver housing, and each
blue robot frame retains silver articulated arms and a red head light. The
asset validator treats those palette motifs as part of the cart contract.

`title-source.png` is an enlarged native-pixel cover master constrained to the
PICO-8 palette, not a smooth illustration intended for downsampling.
`title-screen-pico.png` is its reviewed 128x128 PICO-palette conversion. The
compressed title payload is decoded directly into screen memory, leaving the
gameplay sprite bank independent and fully editable.

After editing a grid, rebuild and validate:

```powershell
python tools/build_blocks_buttons_robot_assets.py
python tools/pico8_title_assets.py --source pico8/blocks-buttons/title-source.png --cart pico8/blocks-buttons/blocks-buttons.p8 --preview pico8/blocks-buttons/title-screen-pico.png
python tools/pico8_title_assets.py --check --source pico8/blocks-buttons/title-source.png --cart pico8/blocks-buttons/blocks-buttons.p8 --preview pico8/blocks-buttons/title-screen-pico.png
python tools/build_blocks_buttons_robot_assets.py --check
python tools/pico8_arcade_build.py validate pico8/blocks-buttons/blocks-buttons.p8
```

The native high-resolution robot masters remain a separate art family and are not
downsampled into this cart.

The cart consumes the tracked 30-room campaign in
`game-design/blocks-buttons/campaign.json`. A undoes one active-play step;
dead-square failures use `Crate stuck`; progress resumes the room, robot,
crates, moves, and pushes.
