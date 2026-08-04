# Runtime Package v1

Status: implemented Windows preview contract

This specification records the local package boundary exercised by Snake. It is not the future signed `.sprout` distribution format.

The reusable engine direction and planned lifecycle evolution are documented under [Sprout Runtime](../runtime/README.md). Those documents distinguish current v1 behavior from proposed versioned slices; this file remains authoritative for implemented v1 packages.

## Package directory

```text
manifest.json
game.lua
asset-manifest.json   optional
assets/               optional runtime PNG atlases
LICENSE
```

The runtime canonicalizes the package root and entrypoint, rejects traversal and symlink escape, accepts text Lua only, and limits a manifest to 64 KiB. Unknown or duplicate manifest fields fail validation.

## Manifest

| Field | Rule |
| --- | --- |
| `schemaVersion` | Integer `1` |
| `id` | Lowercase dotted or hyphenated stable identity |
| `title` | Non-empty display text |
| `version` | `MAJOR.MINOR.PATCH` |
| `runtimeVersion` | Integer `1` |
| `entrypoint` | Relative `.lua` file contained by the package root |
| `assetManifest` | Optional relative `.json` asset manifest contained by the package root |
| `audience` | `family` for child-visible packages or `parent` for parent-only packages |
| `logicalResolution` | Per-game logical width and height, each 64–4096; independent of the physical display target |
| `titleScreen` | Optional full-screen art with a baked game title plus a normalized runtime-control region |
| `libraryArtwork` | Optional wide cover image for launcher library rails |
| `capabilities` | Unique subset of `events` and `local-storage` |

## Lifecycle

A game defines four global functions:

- `init()` initializes a new runtime session.
- `update(actions)` advances exactly one fixed simulation tick.
- `render()` submits the current frame without advancing time.
- `snapshot()` returns deterministic text suitable for state comparison in tests.

A package may additionally define `title_status()` and `reset_progress()`.
When both are present, the native title screen displays the short status text
and offers a host-timed three-second secondary-action hold. The host owns input
consumption and progress presentation; the package owns which persisted values
are reset. Releasing before completion makes no change.

Each lifecycle call has a 500,000-instruction limit, sufficient for bounded
dense-level generation while still terminating runaway package scripts. The
Windows host calls `update` at 60 fixed ticks per second and limits catch-up
after a stall. The runtime controls randomness; identical seeds and action
frames must produce identical snapshots.

The `actions` table contains boolean `up`, `down`, `left`, `right`, `primary`, `secondary`, `start`, and `back` fields. Games target actions rather than keyboard keys or controller button numbers.

## API exercised by Snake

| Function | Behavior |
| --- | --- |
| `sprout.random(maximum)` | Returns a deterministic integer from 1 through `maximum` |
| `sprout.rect(x, y, width, height, r, g, b, a?)` | Submits an in-bounds rectangle on the logical surface |
| `sprout.circle(x, y, radius, r, g, b, a?)` | Submits an alpha-blended filled circle on the logical surface |
| `sprout.label(text, x, y, width, height, r, g, b, a?)` | Centers bounded printable text using the shared rounded UI font |
| `sprout.sprite(...)` | Submits one declared atlas frame |
| `sprout.animate(...)` | Resolves a declared animation from the fixed session tick |
| `sprout.sprite_batch(items)` | Submits many sprite or animation records in one host call |
| `sprout.tilemap(...)` | Submits compact byte-indexed tiles from a declared tile set |
| `sprout.emit(type, value?)` | Emits `AchievementUnlocked` or `LevelCompleted` when `events` is declared |
| `sprout.storage_get(key, fallback?)` | Reads a package integer from local storage |
| `sprout.storage_set(key, value)` | Atomically writes a package integer when `local-storage` is declared |

Storage keys are bounded ASCII identifiers. The launcher gives the runtime a profile-specific storage root, and the runtime adds the package ID, producing `<launcher-data>/native-games/<profile-id>/<package-id>/storage.json`. Direct runtime development commands use their explicitly supplied storage root and remain separate from launcher profile data.

The runtime, not package code, emits `GameStarted` and `GameExited`. Packages cannot forge policy, time, recommendation, or parent-control events.

## Presentation and resolution

`logicalResolution` belongs to each game. The runtime scales that surface to
the active display, so the Miyoo Mini Plus 640×480 panel is a target profile,
not an engine-wide coordinate system or fidelity ceiling. A future game may
choose a denser logical surface without changing existing packages.

An optional `titleScreen` contains a relative PNG, declared pixel dimensions,
`cover` or `contain` fit, and normalized `[x, y, width, height]` regions on a
0–1000 canvas. The illustration bakes in the exact game title so its lettering
belongs to the game's art direction. Game titles must not be prefixed with
`Sprout`. The host owns the separate `A Start / B Back` control pill so input,
localization, profile state, and resume behavior remain dynamic.

`titleScreen.controlsBackground` and `controlsForeground` are optional RGBA
arrays. They let each game keep its controls within its own palette while the
runtime retains input and localization ownership. `libraryArtwork` contains a
relative PNG, declared dimensions, and `cover` or `contain` fit. It is separate
from title art because the library consumes a wide cover-safe composition while
the runtime title page consumes the full 4:3 display.

Title art is stored at the highest reviewed practical resolution and scaled by
the runtime for the active target. The first three packages use 1536×1152 title
images while rendering cleanly at 640×480. Device packaging may later add
explicit lower-memory variants; the package contract does not impose a
640×480 asset ceiling.

The complete manifest, bounds, deterministic animation, and backend batching
contract is [Runtime Assets v1](runtime-assets-v1.md).

## Execution boundary

The interpreter opens only Lua base, table, string, math, and UTF-8 libraries. File, operating-system, dynamic-loading, package, and debug libraries are unavailable; text loading and lifecycle instruction limits reduce accidental and casual package abuse. There is not yet a memory quota or accepted hostile-package sandbox. Only checked-in local packages are supported.

The launcher revalidates the manifest at launch, uses only its configured runtime executable, and never treats package metadata as an executable command. Remote catalogue metadata, archives, hashes, signatures, downloads, updates, release channels, and rollback remain outside v1.
