---
name: sprout-native-game
description: Implement, refactor, debug, or extend a native Sprout Arcade game using Sprout Runtime. Use for files under games/, native Lua packages, package manifests, shared runtime services, high-resolution assets, fixed-step simulation, action input, sprite batching, profile persistence, Windows preview, and reusable engine extraction.
---

# Sprout native game

Build native games as thin rule packages over the reusable runtime.

## Inspect before changing structure

1. Read `docs/runtime/README.md`, `docs/runtime/engine-systems.md`, and `docs/runtime/game-lifecycle.md`.
2. Read `docs/specs/runtime-package-v1.md` and `docs/specs/runtime-assets-v1.md`.
3. Inspect the target package, its manifest, its asset manifest, and at least one comparable package.
4. Read `references/native-contract.md`.

## Preserve ownership boundaries

- Put SDL, filesystem, process, policy, resource bounds, atlas loading, batching, input edges, and durable storage in the runtime.
- Put rules, scoring, level interpretation, scene composition, and game-specific generation in the package.
- Put expensive search, solver work, campaign analysis, and content validation in developer tools.
- Extract a shared module only for a platform/security boundary or two concrete consumers.
- Never create a generic helper whose only consumer is the current game.

## Implement deterministically

- Advance simulation on the fixed update; never mutate state from rendering.
- Consume action-level held, pressed, and released states rather than platform keys.
- Derive named random streams from the launch seed when randomness affects rules.
- Keep checkpoints versioned, bounded, atomic, and profile/package isolated.
- Route return through the host lifecycle; do not terminate the process from game code.

## Render at the package's declared fidelity

- Do not bake 320x240, 640x480, or a universal logical grid into the engine.
- Retain the highest practical reviewed masters and declare runtime target sizes explicitly.
- Use atlas IDs, animation clips, sprite batches, tilemaps, and shared text instead of per-frame path access or one-off glyph tables.
- Bottom-anchor raised grid actors and sort them by ground contact.
- Keep collision geometry independent from visual overscan.
- Use `$sprout-game-assets` for generation, atlas integration, and visual validation.

## Complete the game slice

Implement title/continue, gameplay, every real terminal state, replay/reset, pause/return behavior, persistence, and deterministic capture hooks. Keep capture hooks inert during ordinary play.

## Verify

1. Run the package-appropriate build and tests.
2. Run asset builders with `--check`.
3. Capture title, representative gameplay, success, and every real failure/recovery state with `tools/capture-visual-validation.ps1`.
4. Review captures at native output size.
5. Use `$sprout-game-qa` for the full evidence matrix and `$sprout-game-deploy` before a device claim.
