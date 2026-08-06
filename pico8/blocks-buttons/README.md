# Blocks & Buttons PICO-8 Edition

The canonical PICO artwork is `sprites.json`. Every frame is a literal palette
index grid: fifteen 16x16 robot frames plus hand-authored floor, wall, button,
crate, and solved-crate tiles. East-facing robot frames mirror west at runtime.
The 8x8 board renders at an exact 16 pixels per cell and occupies the complete
128x128 display; no HUD strip reduces the playfield.

`title-source.png` is the full-resolution illustrated cover source.
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

The native 256x256 robot masters remain a separate art family and are not
downsampled into this cart.
