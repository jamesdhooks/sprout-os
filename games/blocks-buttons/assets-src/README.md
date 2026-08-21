# Blocks & Buttons asset source

The fallback atlas is deterministic original pixel art produced by
`tools/generate-arcade-assets.py` using the `blocks-buttons-v1` palette. Run the
tool after changing its source definitions; `--check` verifies byte-for-byte
reproducibility.

The rich artwork uses the engine-neutral `sprite-atlas.v1` interchange format.
`atlas-import.json` combines those atlases with the fallback primitives and
`tools/import-sprite-atlases.py --check` verifies the runtime manifest.

`tools/build_blocks_buttons_robot_assets.py` consumes 24 reviewed transparent
high-angle masters in `robot/`: four foot-motion frames and four push frames
for north, south, and west. The source family follows the character contract in
`game-design/blocks-buttons/README.md`; the builder supplies deterministic
fallback art only when a source is absent.
East deliberately mirrors west at runtime, avoiding a redundant horizontal
source family. `tools/build_blocks_buttons_robot_assets.py` normalizes those
masters to stable 128x160 cells and packs the native atlas. The cart does not
downsample these masters: its 16x16 robot plus floor, wall, button, and crate
frames are hand-refined as hexadecimal pixel grids in
`pico8/blocks-buttons/sprites.json`. The same builder validates and injects
that independent PICO bank. Its `--check` mode verifies exact atlas pixels,
byte-stable metadata, and cartridge payloads.

The world masters in `world/` use a screen-aligned top-down projection.
Floor is a flat walking plane; walls and crates are square; buttons are circular
and visually distinct in raised and pressed states. `tools/build_blocks_buttons_world_assets.py`
packs the native world atlas and its reviewed derivatives. Every object remains
inside its collision cell, and the robot uses a stable bottom-center pivot.
