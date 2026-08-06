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
masters, packs the 1024x1024 native atlas, and derives the cart's 16x16 PICO-8
frames with nearest-neighbour resampling and deterministic palette mapping.
Its `--check` mode verifies exact atlas pixels plus byte-stable metadata and
cartridge payloads, avoiding false failures from PNG encoder-version changes.
