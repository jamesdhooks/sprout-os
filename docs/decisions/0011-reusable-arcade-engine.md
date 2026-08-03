# ADR 0011: Grow the Runtime Through Three Concrete Games

- Status: Accepted
- Date: 2026-08-03

## Context

The Windows preview established a constrained deterministic runtime and one playable Snake package. The prototype proves the process boundary but keeps text drawing, state transitions, input edges, and presentation helpers inside one game script. Copying that pattern across a larger collection would produce inconsistent saves, controls, rendering, difficulty, and lifecycle behavior.

Designing systems for all proposed games now would create speculative engine scope. The first proposed collection has a natural initial sequence: Mouse & Cheese Maze, Blocks & Buttons, and Snake. Together they exercise deterministic grid content, solver-backed campaigns, and real-time endless play.

## Decision

Evolve Sprout Runtime as a reusable engine through those three concrete consumers. The runtime owns deterministic and platform-sensitive services: lifecycle, input frames, named RNG streams, render commands, asset lookup, shared text, checkpoints, persistence, and validated events. Reviewed shared Lua modules may provide state, grid, campaign, UI, and animation utilities under a versioned engine namespace.

Game packages own rules, difficulty parameters, generated/resolved content, and game-specific metrics. Offline generators and solvers remain developer tools and export validated package content. A shared abstraction requires a platform/security boundary or at least two real consumers.

The first three games must be completed end to end before implementation expands to the remaining catalogue. Windows remains the only verified target during this phase.

## Consequences

- Snake will be migrated rather than treated as the permanent engine template.
- Mouse Maze establishes campaign, content, asset, and grid slices; Blocks & Buttons proves they are reusable and adds solver-backed content; Snake completes real-time, daily-seed, and score/checkpoint behavior.
- Shared fonts, storage codecs, host wrappers, and lifecycle state are not copied into packages.
- Package/runtime APIs need explicit versioning and migrations as v2 replaces prototype-only contracts.
- Browser, Linux, Raspberry Pi, Miyoo, and Onion support remain validation work, not implied portability claims.
- The remaining twelve candidate games stay deferred until the first three and their engine evidence are complete.
