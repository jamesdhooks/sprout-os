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
- Texture filtering follows the declared art profile. Storybook raster art uses smooth resampling; deliberately pixel-based packages may request nearest-neighbor scaling later.
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

Initial asset sets are intentionally small:

| Game | Required v1 elements |
| --- | --- |
| Mouse Maze | Mouse in four directions, cheese, wall/floor tiles, goal celebration marks |
| Blocks & Buttons | Player, crate, button, crate-on-button, wall/floor, deadlock/retry marker |
| Snake | Head/body/tail turns, fruit, board tiles, collision/end treatment |

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

Generated atlas bytes must be reproducible from tracked source assets. CI validates and repacks; it fails on a dirty result. Visual review checks the actual runtime at 320×240, not only enlarged contact sheets.

The implemented `tools/generate-arcade-assets.py --check` test verifies the
Mouse Maze and Blocks & Buttons PNGs byte for byte. Their strict manifests
validate atlas dimensions, source rectangles, animation clips, tile sets,
palette name, source category, license, and generator. Windows captures check
the actual runtime at 320×240.

The general runtime contract and actual backend batching behavior are specified
in [Runtime Assets v1](../specs/runtime-assets-v1.md). Atlas packing, preview
contact sheets, and a broader unused-asset audit remain future tooling; they are
not claimed complete.

## Safety and repository hygiene

Do not commit production credentials, private references, working-session transcripts, bulk rejected drafts, or assets with unclear rights. Package licenses cover original game assets unless a more specific compatible license and attribution is recorded.
