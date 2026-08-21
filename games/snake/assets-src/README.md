# Snake asset source

The rich artwork uses the engine-neutral `sprite-atlas.v1` interchange format.
`atlas-import.json` combines the character and world atlases, and
`tools/import-sprite-atlases.py --check` verifies the runtime manifest.

The reviewed `snake/` source family contains one north-facing head, one
horizontal body, one west/north corner, and one north-facing tail. The exact
rotational variants are built--not independently redrawn--by
`tools/build_snake_character_assets.py`, which also opens declared connector
edges to prevent seams. Existing fruit masters remain part of the same
deterministic 4x4 native character atlas.

The PICO-8 edition is an independent hand-authored 8x8 set in
`pico8/snake/sprites.json`. It defines exact connector spans for heads, straight
segments, corners, tails, fruit, ground, flowers, and leaves. The builder
validates and injects those pixels without resampling the native artwork.
