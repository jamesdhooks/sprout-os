# Arcade Catalogue

Status values describe repository truth: **Prototype**, **Planned**, **Deferred**, or **Complete**. A game is complete only when its v1 acceptance criteria, automated evidence, assets, package metadata, and documented Windows play check all pass.

## Proposed first collection

| Order | Game | Content model | Engine systems introduced | Status |
| --- | --- | --- | --- | --- |
| 1 | Mouse & Cheese Maze | Existing native campaign and separate 100-level PICO campaign | Grid, tilemap, movement interpolation, BFS hinting | Prototype |
| 2 | Blocks & Buttons | 30 offline-generated and solver-verified rooms | Occupancy, undo, dead-square detection, solver metadata | Prototype |
| 3 | Starlight Snake | Round and Endless modes | Fixed-step movement, direction queue, connected-body rendering, scoring | Prototype |
| 4 | Key & Door | Resolved generated campaign | Collectibles, conditional tiles, staged objectives | Deferred |
| 5 | Paint the Floor | Resolved generated campaign | Mutable tiles, visited coverage, completion conditions | Deferred |
| 6 | Ice Slide | Solver-verified campaign | Slide-until-collision, stop graphs, turn solver | Deferred |
| 7 | Complete the Pattern | Deterministic generated questions | Prompt/answer framework, rule metadata | Deferred |
| 8 | Find the Odd One | Asset-metadata-driven questions | Cursor grid, visual variants, matching | Deferred |
| 9 | Falling Catch | Seeded event schedules | Timed entities, lanes, round scheduler | Deferred |
| 10 | Frog Crossing | Validated lane schedules | Moving hazards, wraparound lanes, timing validation | Deferred |
| 11 | Reach the Flag | Offline-validated platform layouts | Continuous motion, jump physics, platform collision | Deferred |
| 12 | Keep It Up | Seeded endless run | Impulses, overlap, simple continuous physics | Deferred |
| 13 | Helicopter Cave | Seeded terrain stream | Scrolling world, bounded procedural terrain | Deferred |
| 14 | Area Capture | Seeded starting states | Bouncing bodies, growing walls, region fill | Deferred |
| 15 | Group Clearing | Validated seeded boards | Connected components, tile gravity, column collapse | Deferred |

Traffic Lights remains an optional later candidate after moving-entity scheduling has two proven consumers.

## Sequence discipline

The first three games are the committed implementation scope. Later rows define direction, not active commitments. Each later game requires a focused design review against the engine that actually exists at that time.

The sequence intentionally grows from deterministic tile navigation, to solver-backed spatial puzzles, to real-time endless play. It must not produce fifteen copied input loops, fonts, storage formats, state machines, or render helpers. Shared systems and their ownership are defined in [Runtime Engine Systems](../runtime/engine-systems.md).

## Active tracking

| Work | Issue |
| --- | --- |
| Documentation and architecture foundation | [#39](https://github.com/jamesdhooks/sprout-os/issues/39) |
| Deterministic engine core | [#40](https://github.com/jamesdhooks/sprout-os/issues/40) |
| Assets and reusable renderer | [#41](https://github.com/jamesdhooks/sprout-os/issues/41) |
| Offline content pipeline | [#42](https://github.com/jamesdhooks/sprout-os/issues/42) |
| Mouse & Cheese Maze | [#43](https://github.com/jamesdhooks/sprout-os/issues/43) |
| Blocks & Buttons | [#44](https://github.com/jamesdhooks/sprout-os/issues/44) |
| Snake completion | [#45](https://github.com/jamesdhooks/sprout-os/issues/45) |
| End-to-end first-three evidence | [#46](https://github.com/jamesdhooks/sprout-os/issues/46) |

## Common game contract

Every completed game must provide:

- offline launch and play;
- profile-scoped save/progress data;
- explicit difficulty parameters and recorded difficulty metrics;
- deterministic seeds for generated content;
- instant restart or next-level flow;
- primary action to continue/retry and secondary action to return;
- host-owned menu, pause, screen-time, and exit behavior;
- structured level, achievement, and score events where applicable;
- suspend/resume from a runtime-owned checkpoint;
- a package license and asset provenance record;
- deterministic tests plus a Windows visual/controller checklist.

Favorites and recents belong to the launcher library, not individual game scripts.
