# Arcade Asset Pipeline

## Resolution and presentation

Artwork begins from the highest practical reviewed master. Runtime derivatives
are selected per target profile; 640×480 is the Miyoo Mini Plus display target,
not a global game canvas. Each package independently declares its logical
resolution, and asset scale/density metadata must remain explicit.

SproutOS startup art, launcher art, and per-game title art are distinct assets.
A game title background is full-screen and theme-consistent with its gameplay
art, while the runtime owns the title and simple Start/Continue/Back interface.
This keeps text crisp, localized, and state-aware while allowing rich artwork.
The launcher likewise renders profile and navigation UI over its own reviewed
4:3 master and uses a standard `sprite-atlas.v1` sheet for small decorative
accents. Neither launcher artwork nor its atlas is reused as a game title page.

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

`assets-src/` may contain layered originals, palette files, SVG source, or reviewed generated concepts. Runtime packages contain normalized raster output only. Temporary generations and rejected variants remain outside Git.

## First-collection visual standard

- Base grid: 8×8 or 16×16 pixels; larger sprites use exact multiples.
- Integer nearest-neighbor scaling only.
- A small named palette with sufficient contrast for foreground, background, focus, success, and danger states.
- Shape and animation distinguish important state; color alone is insufficient.
- One- or two-frame idle art is acceptable. Animation must serve input feedback or state readability.
- Text uses one shared engine font/atlas; games do not embed private glyph tables.
- Atlas padding and transparent pixels are validated to prevent sampling artifacts.

Initial asset sets are intentionally small:

| Game | Required v1 elements |
| --- | --- |
| Mouse Maze | Mouse in four directions, cheese, wall/floor tiles, goal celebration marks |
| Blocks & Buttons | Player, crate, button, crate-on-button, wall/floor, deadlock/retry marker |
| Sprout Snake | Head/body/tail turns, fruit, board tiles, collision/end treatment |

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

Do not commit model credentials, private references, raw prompt transcripts, bulk rejected generations, or assets with unclear rights. Package licenses cover original game assets unless a more specific compatible license and attribution is recorded.
