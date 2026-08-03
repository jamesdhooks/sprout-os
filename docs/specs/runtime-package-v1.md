# Runtime Package v1

Status: implemented Windows preview contract

This specification records the local package boundary exercised by Sprout Snake. It is not the future signed `.sprout` distribution format.

## Package directory

```text
manifest.json
game.lua
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
| `audience` | `family` for child-visible packages or `parent` for parent-only packages |
| `logicalResolution` | Width 64–640 and height 64–480 |
| `capabilities` | Unique subset of `events` and `local-storage` |

## Lifecycle

A game defines four global functions:

- `init()` initializes a new runtime session.
- `update(actions)` advances exactly one fixed simulation tick.
- `render()` submits the current frame without advancing time.
- `snapshot()` returns deterministic text suitable for state comparison in tests.

Each lifecycle call has a 100,000-instruction limit. The Windows host calls `update` at 60 fixed ticks per second and limits catch-up after a stall. The runtime controls randomness; identical seeds and action frames must produce identical snapshots.

The `actions` table contains boolean `up`, `down`, `left`, `right`, `primary`, `secondary`, `start`, and `back` fields. Games target actions rather than keyboard keys or controller button numbers.

## API exercised by Sprout Snake

| Function | Behavior |
| --- | --- |
| `sprout.random(maximum)` | Returns a deterministic integer from 1 through `maximum` |
| `sprout.rect(x, y, width, height, r, g, b, a?)` | Submits an in-bounds rectangle on the logical surface |
| `sprout.emit(type, value?)` | Emits `AchievementUnlocked` or `LevelCompleted` when `events` is declared |
| `sprout.storage_get(key, fallback?)` | Reads a package integer from local storage |
| `sprout.storage_set(key, value)` | Atomically writes a package integer when `local-storage` is declared |

Storage keys are bounded ASCII identifiers. The launcher gives the runtime a profile-specific storage root, and the runtime adds the package ID, producing `<launcher-data>/native-games/<profile-id>/<package-id>/storage.json`. Direct runtime development commands use their explicitly supplied storage root and remain separate from launcher profile data.

The runtime, not package code, emits `GameStarted` and `GameExited`. Packages cannot forge policy, time, recommendation, or parent-control events.

## Execution boundary

The interpreter opens only Lua base, table, string, math, and UTF-8 libraries. File, operating-system, dynamic-loading, package, and debug libraries are unavailable; text loading and lifecycle instruction limits reduce accidental and casual package abuse. There is not yet a memory quota or accepted hostile-package sandbox. Only checked-in local packages are supported.

The launcher revalidates the manifest at launch, uses only its configured runtime executable, and never treats package metadata as an executable command. Remote catalogue metadata, archives, hashes, signatures, downloads, updates, release channels, and rollback remain outside v1.
