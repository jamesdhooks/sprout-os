# Arcade Asset Pipeline

## Resolution and presentation

Artwork begins from the highest practical reviewed master. Runtime derivatives
are selected per target profile; 640×480 is the Miyoo Mini Plus display target,
not a global game canvas. Each package independently declares its logical
resolution, and asset scale/density metadata must remain explicit.

SproutOS startup art, launcher art, and per-game title art are distinct assets.
A game title page is full-screen and theme-consistent with its gameplay art.
Its exact game title is part of the illustration; the runtime owns only the
simple Start/Continue/Back control surface. No game title uses `Sprout` as a
prefix.
The launcher likewise renders profile and navigation UI over its own reviewed
4:3 master and uses a standard `sprite-atlas.v1` sheet for small decorative
accents. Neither launcher artwork nor its atlas is reused as a game title page.

Every game also supplies a 16:9 library cover derivative or reviewed master.
Cover art, title art, and gameplay assets share that game's characters and
palette but have separate composition contracts.

Status: **First deterministic atlases and runtime pipeline implemented**

Sprout Arcade uses original, package-owned artwork with deterministic runtime
packing. Visual assets must remain readable at each package's declared logical
resolution and at the 640×480 handheld target without imposing one global art
style, pixel density, or coordinate system on future games.

## Source and runtime layout

```text
games/<game>/
├── assets-src/       editable originals and generation inputs
├── assets/           runtime-ready indexed PNG atlases
├── asset-manifest.json
└── ATTRIBUTION.md    required when any source needs attribution
```

`assets-src/` may contain layered originals, palette files, SVG source, or other reviewed project artwork. Runtime packages contain normalized raster output only. Temporary production material and rejected variants remain outside Git.

## First-collection visual standard

- Source art retains the highest practical reviewed resolution; atlas cells and runtime target sizes are explicit rather than inferred from one global grid.
- Texture filtering is declared per atlas as `linear` or `nearest`. Optional
  deterministic mips let the runtime choose an appropriate reviewed source
  density without undersampling the requested draw size.
- A coherent named palette provides sufficient contrast for foreground, background, focus, success, and danger states.
- Each game has a distinct named palette and title treatment. The collection
  shares a storybook medium, not one repeated green-and-cream skin.
- Shape and animation distinguish important state; color alone is insufficient.
- One- or two-frame idle art is acceptable. Animation must serve input feedback or state readability.
- Text uses one shared engine font/atlas; games do not embed private glyph tables.
- Atlas padding and transparent pixels are validated to prevent sampling artifacts.
- Directional sprite sheets use uniform cells and center anchors unless a
  documented trimmed-frame contract is required. Runtime capture must confirm
  stable orientation, scale, and silhouette across every frame.
- Grid actors and raised objects use a bottom-center ground-contact pivot. A
  high-angle view may extend above its occupied cell, but its feet or base must
  land on the cell's bottom edge. Cardinal directions remain screen-aligned;
  they do not rotate the collision grid or substitute a diagonal three-quarter
  pose.

Initial asset sets are intentionally small:

| Game | Required v1 elements |
| --- | --- |
| Mouse Maze | Mouse in four directions, cheese, wall/floor tiles, goal celebration marks |
| Blocks & Buttons | Player, crate, button, crate-on-button, wall/floor, deadlock/retry marker |
| Snake | Head/body/tail turns, fruit, board tiles, collision/end treatment |

## First-collection palette contracts

Every newly produced gameplay asset must name its game palette and use only the
listed colours. Production prompts repeat the complete list; automated checks
validate final indexed output rather than trusting prompt intent.

| Game | Palette ID | Exact colours |
| --- | --- | --- |
| Mouse & Cheese | `mouse-cheese-v1` | `#542743 #294B31 #3E612F #647E35 #91A74B #713C2A #9B5332 #C97442 #FFF0BF #FFF8E8 #D8C7B8 #EFA7A3 #C66A25 #F4A62A #FFD75A #C85836` |
| Blocks & Buttons | `blocks-buttons-v1` | `#102F5B #173B70 #16579B #2584CE #39AFCC #66768C #A9BCC8 #E4EEF0 #8F2C36 #D93932 #EF654B #704327 #B86B38 #E4A35B #FFC53B #F6F0DF` |
| Starlight Snake | `starlight-snake-v1` | `#161338 #201B52 #352768 #604285 #F7EBC9 #B8772B #EFB63E #FFDE70 #8F2855 #C83B68 #EE6A91 #3D5FA8 #244A48 #3D7662 #78AA74 #8DE5C2` |

PICO-8 palette contracts separately enumerate allowed standard indices and
extended remaps in `pico8/palettes/standard.json` and each game-design palette
file. PICO gameplay sprites are authored and corrected at their final tiny
resolution; native masters and their mips are independent assets.

## Creation and sourcing

Assets may be original project artwork, commissioned work, or adapted from a compatible source. Every accepted asset must have a known license and source category. Do not imitate a living artist, recognizable game property, trademarked character, or proprietary sprite sheet.

Runtime assets must be normalized to the package's coordinate system, transparency, anchor, and readability constraints. Track only durable runtime provenance in `asset-manifest.json`; raw production files and tool-specific working metadata stay outside the repository.

Example provenance entry:

```json
{
  "id": "mouse.walk.down",
  "source": "original-project-artwork",
  "license": "GPL-3.0-or-later",
  "review": {
    "anchor": "bottom-center",
    "targetScale": "package-defined",
    "derivativeRiskChecked": true
  }
}
```

## Build and validation routines

The first reproducibility routine is implemented; the remaining names describe
the intended tooling surface:

- `asset-validate`: dimensions, grid alignment, palette, alpha, duplicate IDs, manifest coverage, and attribution presence.
- `asset-pack`: deterministic atlas ordering and metadata export.
- `asset-preview`: nearest-neighbor contact sheet at native and 2× scale.
- `asset-audit`: source/license/provenance completeness and unused-asset detection.

Generated atlas bytes must be reproducible from tracked source assets. CI validates and repacks; it fails on a dirty result. Visual review checks each package at its declared logical resolution, not only enlarged contact sheets.

The implemented `tools/generate-arcade-assets.py --check`,
`tools/build_blocks_buttons_robot_assets.py --check`, and
`tools/build_blocks_buttons_world_assets.py --check` tests verify the
reproducible runtime atlases. The PICO-specific hand-authored sources live in
`pico8/blocks-buttons/sprites.json` and `pico8/snake/sprites.json`; their
builders validate exact dimensions, palette indices, non-overlap, and injected
cart bytes. The strict manifests
validate atlas dimensions, source rectangles, animation clips, tile sets,
palette name, source category, license, and generator. Windows captures check
the actual runtime at each package's declared surface, including the 640x480
handheld target used by the first native collection.

The general runtime contract and actual backend batching behavior are specified
in [Runtime Assets v1](../specs/runtime-assets-v1.md). Atlas packing, preview
contact sheets, and a broader unused-asset audit remain future tooling; they are
not claimed complete.

## Safety and repository hygiene

Do not commit production credentials, private references, working-session transcripts, bulk rejected drafts, or assets with unclear rights. Package licenses cover original game assets unless a more specific compatible license and attribution is recorded.
