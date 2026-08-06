---
name: sprout-pico8-game
description: Create, implement, debug, optimize, test, or revise a standalone Sprout PICO-8 game. Use for .p8 cartridges, PICO Lua, 128x128 rendering, sprite/map/SFX/music banks, cartdata persistence, token and compressed-cart budgets, deterministic capture states, desktop PICO-8, fake-08 compatibility, and Sprout PICO catalogue integration.
---

# Sprout PICO-8 game

Treat each cart as a complete PICO-8 game, not a mechanically downscaled native build.

## Load the correct guidance

1. Read `docs/development/pico8-game-development.md`.
2. Read the relevant portions of `docs/development/pico8-authoring-handbook.md`.
3. Read `docs/arcade/pico8-editions.md`, the target cart, its manifest, and its README.
4. Read `references/cart-contract.md`.
5. For unfamiliar PICO-8 mechanics, use `references/tutorial-routing.md` to select the smallest relevant tutorial.
6. For movement, collision, animation, feedback, onboarding, or audio work, apply `references/gameplay-patterns.md`.

## Start and structure

Use:

```powershell
python tools/pico8_game.py new <slug> --title "<Title>"
python tools/pico8_game.py validate <slug>
python tools/pico8_game.py run <slug>
```

Keep the release cart self-contained. Do not require `#include`, generators, local paths, the native runtime, or external assets on Miyoo.

## Respect PICO-8 as its own target

- Render at fixed 128x128.
- Budget code, compressed cart data, sprite memory, map memory, SFX, and music deliberately.
- Use dedicated PICO artwork and audio; do not shrink native masters and call them finished.
- Keep title art separate from gameplay sprite-bank needs when the existing compressed title payload workflow applies.
- Use exact palette-index sources and review at 1x logical resolution plus integer nearest-neighbour enlargement.
- Choose sprite dimensions from gameplay readability. Raised objects may exceed their logical cell and remain bottom-anchored.

## Implement the complete loop

- Use `_update60()` for simulation and `_draw()` only for presentation.
- Use D-pad and PICO buttons; avoid keyboard-only release controls.
- Make B return to the previous screen/title without erasing data.
- Require a deliberate hold with visible, cancellable progress for destructive reset.
- Provide clear title, play, success, failure when applicable, replay, and route-home behavior.
- Store schema version in persistent data and reset or migrate only this cart on incompatibility.
- Keep public and Sprout-managed per-profile `cartdata()` identifiers legal and isolated.

## Share carefully

Share design rules and campaign provenance with a native edition, never Lua runtime code or rendered assets. Inject shared PICO source only when multiple carts use it and the release cart remains standalone.

## Build and verify

```powershell
python tools/pico8_arcade_build.py inject <cart>
python tools/pico8_arcade_build.py validate <cart>
python tools/pico8_game.py capture <slug> --state title
python tools/pico8_game.py capture <slug> --state gameplay
python tools/pico8_game.py capture <slug> --state win
```

Add `fail` only for a real failure state. Confirm the licensed PICO-8 compiler's token/cart report, storage round-trip, held input, B/back, pause/exit, and representative early/middle/late content. Use `$sprout-game-qa` and then `$sprout-game-deploy`.
