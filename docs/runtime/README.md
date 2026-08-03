# Sprout Runtime

Status: **Windows v1 engine foundation in progress**

Sprout Runtime is the portable host and reusable 2D game engine for Sprout Arcade packages. It owns platform integration, deterministic sessions, action input, rendering commands, asset access, profile-scoped persistence, structured events, and lifecycle control. Packages own game rules, content, progression tuning, and game-specific generators.

## Current implementation

Runtime v1 provides a constrained Lua session, fixed 60 Hz stepping,
action-level input, seeded randomness, rectangle rendering, strict atlas and
animation manifests, sprite and animation batches, compact tilemaps, lazy PNG
texture caching, geometry-coalesced SDL rendering, bounded integer storage, two
package events, deterministic snapshots, and strict local manifests. Mouse
Maze, Blocks & Buttons, and Snake are discoverable Windows packages; the first
two exercise the shared asset renderer.

The renderer/asset slice is implemented, but the broader first-collection
engine is not complete. Snake still contains its own text renderer, state
transitions, and drawing helpers; shared text, campaign content,
suspend/restore, named RNG streams, and read-only package content remain open.

## Target structure

```text
Platform host
  SDL window, controller, filesystem adapter, process lifecycle
        ↓
Deterministic session kernel
  fixed clock, named RNG streams, input frames, lifecycle, checkpoints
        ↓
Reusable engine services
  renderer, assets, text, grid/occupancy, state flow, campaign, events
        ↓
Game package
  rules, content, difficulty parameters, game-specific generation metadata
```

The platform host must not contain game rules. The engine must not contain a maze generator, Sokoban solver, Snake scoring rules, or other game-specific logic. Offline generators and solvers live in developer tooling and export validated content.

## Documentation map

- [Engine systems](engine-systems.md): ownership, shared modules, and implementation order.
- [Game lifecycle](game-lifecycle.md): deterministic update, save/restore, exit, and event contracts.
- [Runtime Package v1](../specs/runtime-package-v1.md): currently implemented manifest and API.
- [Runtime Assets v1](../specs/runtime-assets-v1.md): atlas, animation, tilemap, and batching contract.
- [Events](../specs/events.md): shared vocabulary and ownership.
- [Arcade content pipeline](../arcade/content-pipeline.md): offline generation and campaign build.
- [Arcade assets](../arcade/assets.md): asset source, packing, and provenance.

The API evolves in versioned slices driven by Mouse Maze, Blocks & Buttons, and Snake. Compatibility migrations must be explicit; packages never silently receive changed deterministic behavior.
