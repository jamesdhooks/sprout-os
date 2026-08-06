# Snake asset source

The rich artwork uses the engine-neutral `sprite-atlas.v1` interchange format.
`atlas-import.json` combines the character and world atlases, and
`tools/import-sprite-atlases.py --check` verifies the runtime manifest.

The reviewed `snake/` source family contains one north-facing head, one
horizontal body, one west/north corner, and one north-facing tail. The exact
rotational variants are built—not independently redrawn—by
`tools/build_snake_character_assets.py`, which also opens declared connector
edges to prevent seams and derives the PICO-8 sprite bank. Existing fruit
masters remain part of the same deterministic 4x4 character atlas.
