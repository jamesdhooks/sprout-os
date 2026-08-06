# Blocks & Buttons PICO-8 Edition

The canonical PICO artwork is `sprites.json`. Every frame is a literal palette
index grid: fifteen 16x16 robot frames plus hand-authored floor, wall, button,
crate, and solved-crate tiles. East-facing robot frames mirror west at runtime.

After editing a grid, rebuild and validate:

```powershell
python tools/build_blocks_buttons_robot_assets.py
python tools/build_blocks_buttons_robot_assets.py --check
python tools/pico8_arcade_build.py validate pico8/blocks-buttons/blocks-buttons.p8
```

The native 256x256 robot masters remain a separate art family and are not
downsampled into this cart.
