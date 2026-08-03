# Mouse Maze asset source

The fallback atlas is deterministic original pixel art produced by
`tools/generate-arcade-assets.py` using the `sprout-arcade-1` palette. Run the
tool after changing its source definitions; `--check` verifies byte-for-byte
reproducibility.

The rich artwork uses the engine-neutral `sprite-atlas.v1` interchange format.
`atlas-import.json` combines those atlases with the fallback primitives and
`tools/import-sprite-atlases.py --check` verifies the runtime manifest.
