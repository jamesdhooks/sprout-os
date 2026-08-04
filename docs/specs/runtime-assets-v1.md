# Runtime Assets v1

Status: implemented on the Windows runtime

This contract lets reviewed game packages use deterministic sprite atlases
without receiving filesystem or SDL access. The package manifest names one
relative `assetManifest`; the runtime canonicalizes that file and every atlas
image inside the package root.

## Asset manifest

The manifest has five required sections:

| Section | Purpose |
| --- | --- |
| `provenance` | Source category, license, deterministic generator, and named palette |
| `atlases` | Stable atlas ID, relative PNG path, and declared pixel dimensions |
| `sprites` | Stable frame ID, atlas, source rectangle, and anchor pivot |
| `animations` | Ordered sprite frames with fixed tick durations and loop behavior |
| `tileSets` | Ordered sprite frames addressable by byte index and resampled to an explicit logical tile size |

IDs are package-local. Atlas images are loaded lazily, decoded once, checked
against their declared dimensions, retained for the session, alpha blended,
and sampled with smooth linear scaling. Source rectangles must fit their
atlas. Animation frames and tile-set entries must reference declared sprites.

The loader bounds manifests to 512 KiB, atlases to 16, sprites to 2,048,
animations to 512, animation frames to 128 per clip, tile sets to 128, atlas
edges to 4,096 pixels, and each rendered frame to 4,096 commands.

## Lua rendering API

| Function | Use |
| --- | --- |
| `sprout.sprite(id, x, y, scale?, flipX?, flipY?, alpha?)` | Submit one declared frame; fractional scale supports high-density masters |
| `sprout.animate(id, x, y, phase?, scale?, flipX?, flipY?, alpha?)` | Resolve one declared animation with the same fractional scale contract |
| `sprout.sprite_batch(items)` | Submit many sprite or animation items in one Lua-to-host call |
| `sprout.tilemap(tileSet, bytes, columns, x, y, scaleX?, scaleY?, skipIndex?)` | Submit a dense tile grid with independent axes and an optional omitted tile index |
| `sprout.label(text, x, y, width, height, r, g, b, a?)` | Submit one bounded shared-font label without a package-specific glyph atlas |

A batch item names exactly one `sprite` or `animation` and supplies integer
`x` and `y`. Optional fields are `phase`, `scale`, `flipX`, `flipY`, and
`alpha`. Animation selection derives only from the fixed session tick plus an
explicit phase; rendering never advances animation state.

Labels scale the shared font against both available width and height and clip
glyph output to the declared region. Dynamic or localized text must therefore
remain inside its package-owned background rather than overflowing at a fixed
font height.

Sprite scale is a finite number from 1/64 through 64. This permits a dense
source frame to render on a compact logical canvas without discarding its
higher-density source or forcing every future game to share one pixel scale.
Tilemaps accept finite fractional X/Y scales from 1/64 through 8 and fractional
origins. When Y scale is omitted it inherits X scale. The host rounds shared
tile edges from cumulative positions rather than rounding each tile
independently, so fractional scales remain contiguous without seams or overlap.
Independent axes let a package map a complete logical grid to its viewport
without inventing extra padding; actor sprites can still use uniform scale.
Source frames may be larger than the logical tile. The host resamples each frame
to the scaled tile geometry, allowing one rich atlas to serve different board
densities and future higher-density packages without baking a low-resolution
source grid.

Tilemap bytes are zero-based indices into the declared tile set. This compact
representation avoids one Lua call per tile and makes invalid tile values fail
at the package boundary. An optional `skipIndex` from 0 through 255 omits that
valid tile value while retaining every cell's position. A package can therefore
submit a floor pass, actors, and a masked wall pass so foreground geometry
occludes sprites without expanding the map into per-tile Lua calls.

## Backend batching

The session produces an ordered, frame-local command buffer. The SDL backend
coalesces each consecutive run of sprites from the same atlas into one indexed
`SDL_RenderGeometry` submission. Every sprite contributes four vertices and
six indices, including per-vertex alpha/tint and flipped UVs. A rectangle or
atlas change ends the current batch, preserving painter order. A typical Maze
frame therefore crosses Lua once for its tilemap, once for its actor batch, and
submits the atlas geometry in a single backend draw.

Automated core tests inspect commands without creating a window. Windows
capture tests exercise PNG decoding, declared-size validation, smooth atlas
sampling, geometry batching, and the actual SDL output path. The current host
resamples from the full-resolution atlas; it does not yet select from an
explicit mip chain.

## Common atlas imports

`tools/import-sprite-atlases.py` compiles `sprite-atlas.v1`, TexturePacker JSON
Hash, or TexturePacker JSON Array metadata into the strict runtime manifest.
One import config can combine multiple source atlases, apply stable ID prefixes
or explicit renames, preserve normalized pivots, translate animation FPS to
fixed runtime ticks, merge a deterministic base manifest, correct explicitly
declared row/column ordering, and declare package tile sets. The compiler
supports `--check` for reproducible builds. Runtime packages therefore retain
one small, validated contract without requiring artists or third-party tools to
export a Sprout-specific layout directly.

## Reproducible first assets

`tools/generate-arcade-assets.py` creates the original Mouse Maze and Blocks &
Buttons atlases using only deterministic integer pixel operations and the
`sprout-arcade-1` palette. `--check` compares generated bytes with the tracked
PNGs and is registered as a CTest. The per-game manifest records license and
generator provenance; `assets-src/README.md` identifies the editable source.
