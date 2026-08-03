# Runtime Engine Systems

Status: **Asset renderer implemented; remaining reusable slices in progress**

The runtime should become a compact reusable engine without becoming a catalogue of game-specific helpers. A shared module is justified when it crosses a platform/security boundary or has at least two concrete game consumers. Until then, keep the logic in the game or offline tool where it can be changed cheaply.

## Ownership

| System | Runtime/engine owns | Game/tool owns | First consumers |
| --- | --- | --- | --- |
| Session kernel | Fixed update clock, pause, resume, exit, deterministic frame number | Game state transitions | All games |
| Input | Held/pressed/released actions, repeat policy, controller mapping | Meaning of an action in a game state | All games |
| Randomness | Stable named streams derived from launch seed; serializable stream state | Which stream and distribution a rule uses | Maze, Snake, later generated games |
| Renderer | Bounded commands, integer coordinates, clipping, palette/color, sprite/tile/text submission | Scene composition and visual rules | All games |
| Assets | Manifest validation, read-only asset IDs, atlas loading, nearest-neighbor sampling | Original assets and animation definitions | All games |
| Text | Shared bitmap font, alignment, measurement, numeric formatting primitives | Game copy and placement | All games |
| Grid | Dense tile/occupancy storage, bounds, neighbors, coordinate transforms | Maze walls, crate rules, Snake body rules | First three games |
| State flow | Small scene/state stack, transition timing, primary-to-continue convention | Game-specific states and transition decisions | First three games |
| Campaign | Read-only content lookup, content identity, progress cursor | Generated layouts, metrics, unlock order | Maze, Blocks & Buttons |
| Persistence | Versioned profile checkpoint and durable progress/high-score records | Game schema and migration callback | All games |
| Events | Validate and deliver bounded typed events | Achievement/level/score facts | All games |
| Difficulty | Store explicit parameter/metric records and active profile selection | Game-specific parameters and scoring | All games |

Grid utilities may provide bounds, indexing, neighbor iteration, occupancy, and coordinate conversion. They must not implement DFS mazes, crate deadlocks, or fruit placement; those remain concrete consumers built from the primitives.

## Render command model

The runtime retains a frame-local command buffer and platform backends consume it. The minimum first-collection command set is:

- clear logical surface;
- filled rectangle for fallback/debug UI;
- sprite region with integer position, optional flip, and palette/tint;
- tilemap region with clipping;
- shared-font text with measured alignment; and
- clip push/pop.

Packages reference asset IDs declared in their manifest. They cannot open paths or upload arbitrary runtime textures. Automated tests can inspect commands without creating an SDL window.

The implemented v1 renderer loads strict package-local atlas manifests,
resolves fixed-tick animations, accepts general sprite/animation batches and
compact byte-indexed tilemaps, and coalesces same-atlas command runs into one
indexed SDL geometry submission. See [Runtime Assets v1](../specs/runtime-assets-v1.md).

## Package code reuse

Shared Lua modules are loaded by the runtime from reviewed engine resources and exposed under a versioned namespace such as `sprout.engine.v2`. Packages do not copy those files and do not gain general filesystem-backed `require`, `load`, or `dofile` access.

Candidate engine modules:

```text
state       scene transitions and edge-triggered actions
grid        coordinates, dense cells, occupancy, neighbors
campaign    resolved content lookup and progression cursor
ui          shared title/result panels and text layout
animation   deterministic frame selection
```

Modules are added incrementally. The C++ host implements only operations that need platform access, resource control, or efficient batching.

## Determinism and performance

- Simulation runs at a fixed tick; rendering never advances state.
- Named RNG streams prevent an unrelated effect from changing level or fruit sequences.
- Stream algorithm and derivation are versioned and covered by golden vectors.
- Game checkpoints include frame/tick phase and RNG stream state.
- Runtime APIs have explicit bounds on command count, asset dimensions, content size, storage, event count, and lifecycle instructions.
- Tilemaps and sprite batches avoid thousands of Lua-to-C calls on low-power targets; performance budgets are measured before device claims.

## Planned implementation slices

1. Extract session state flow, input edges, shared text, and deterministic RNG streams while keeping the current Snake behavior passing.
2. Add asset manifests and sprite/tile commands for Mouse Maze. **Asset and render portion complete; text, content reads, and checkpoints remain.**
3. Add grid/occupancy primitives and campaign progress used by Maze and Blocks & Buttons.
4. Add tool-side solver metadata and runtime deadlock-result consumption for Blocks & Buttons.
5. Migrate Snake to the shared engine, add daily seeds, speed curves, assets, and full save/restore.

Later game systems are not created until their game enters committed scope.
