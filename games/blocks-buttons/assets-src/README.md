# Blocks & Buttons asset source

The fallback atlas is deterministic original pixel art produced by
`tools/generate-arcade-assets.py` using the `sprout-arcade-1` palette. Run the
tool after changing its source definitions; `--check` verifies byte-for-byte
reproducibility.

The rich artwork uses the engine-neutral `sprite-atlas.v1` interchange format.
`atlas-import.json` combines those atlases with the fallback primitives and
`tools/import-sprite-atlases.py --check` verifies the runtime manifest.

The reviewed robot source consists of fifteen transparent 256x256 masters in
`robot/`: four walk frames and one push frame for north, south, and west.
East deliberately mirrors west at runtime, avoiding a redundant horizontal
source family. `tools/build_blocks_buttons_robot_assets.py` validates those
masters and packs the 1024x1024 native atlas. The cart does not downsample
these masters: its 16x16 robot, floor, wall, button, and crate frames are
hand-authored as hexadecimal pixel grids in
`pico8/blocks-buttons/sprites.json`. The same builder validates and injects
that independent PICO bank. Its `--check` mode verifies exact atlas pixels,
byte-stable metadata, and cartridge payloads.

The four reviewed world masters in `world/` use the same fixed high-angle,
unrotated projection. Floor is a flat walking plane; walls, crates, and buttons
show a shallow south-facing edge. `tools/build_blocks_buttons_world_assets.py`
derives solved/pressed states and packs the native world atlas. Character and
object pivots are bottom-center, and the runtime places that pivot on the exact
bottom edge of the occupied collision cell.
