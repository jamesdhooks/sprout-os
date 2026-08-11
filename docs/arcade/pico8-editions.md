# PICO-8 Editions

Status: **Windows PICO-8 validated; fake-08 and licensed-wrapper process launch verified on Onion; physical acceptance pending**

Sprout keeps PICO-8 editions beside their native counterparts. They share game
rules and visual language, not runtime code or high-resolution assets. Each
cart is a standalone `.p8` file suitable for Onion's PICO/fake-08 emulator.

## Current carts

| Cart | Round completion | Failure | Persistent data |
| --- | --- | --- | --- |
| Mouse & Cheese Maze | Find the cheese and advance through the 100-level campaign | None | Current level and solved count |
| Blocks & Buttons | Complete 30 solver-verified rooms | Precomputed dead square | Room and resumable room state |
| Starlight Snake | Round: 12 fruit; Endless: score chase | Wall or body collision | Mode bests and completed rounds |

Blocks & Buttons uses a strict orthographic top-down toy-workshop renderer
aligned to its square collision grid. Walls, crates, buttons, and the robot
remain inside their occupied cells without perspective overlap. Its robot uses
reviewed north, south, and west walk/push families; east mirrors west at runtime.
The west/east family is screen-cardinal rather than a diagonal three-quarter
pose. Every character, crate, and button uses a stable cell-relative anchor.
The native editions retain high-resolution masters, while the standalone carts
use independent hand-refined palette-index grids. Blocks & Buttons defines a
16x16 robot and workshop props in `pico8/blocks-buttons/sprites.json`.
Starlight Snake defines its heads, straight sections, corners, tails, fruit,
and garden details at 8x8 in `pico8/snake/sprites.json`. Builders validate and
inject these banks without shrinking or palette-fitting the native artwork.
Blocks & Buttons uses an exact 8x8-by-16px board. Starlight Snake uses a 14x14
playable garden inside a one-cell 8px boundary. Both compositions account for
all 128x128 screen pixels and reserve no permanent HUD region.
Blocks & Buttons keeps 16px collision and artwork cells. Square walls and
crates, circular buttons, and the robot therefore remain legible without
rotating the collision board or changing its controls.

Each cart keeps an enlarged native-pixel `title-source.png` separate from its
literal gameplay sprites. The source follows the exact 16-colour PICO palette
and represents a 128x128 logical canvas rather than a smooth illustration.
`tools/pico8_title_assets.py` samples it with nearest-neighbour scaling, writes
a review PNG, and injects a compressed screen payload. The runtime decodes it
only on title entry, so box-art complexity does not consume gameplay atlas
space.

Blocks & Buttons and Starlight Snake share the small lifecycle source in
`pico8/shared/arcade_core.lua`. `tools/pico8_arcade_build.py` injects that
source into each cartridge so deployment never depends on `#include` or an
external file. Game rules and renderers remain cart-owned.

All explicit wins use a pop-animated celebration card. When its display period
ends—or the primary action shortens it—the cart returns to its title and resets
the playable state. Failure cards remain until the player retries or returns.

Each cart declares an allowed subset of the standard PICO-8 palette in its
game-design palette file. Builders and tests reject undeclared sprite-bank
colours. The carts contain original short SFX and one compact music loop where
their cartridge budgets permit. On every title, holding B for three seconds is
cancellable, fires once, shows completion until release, and requires fresh
input afterward.

## Build and review

Refresh and validate shared code:

```powershell
python tools/pico8_arcade_build.py inject pico8/blocks-buttons/blocks-buttons.p8 pico8/snake/snake.p8
python tools/pico8_arcade_build.py validate pico8/blocks-buttons/blocks-buttons.p8 pico8/snake/snake.p8
```

Capture a deterministic review state with a licensed desktop PICO-8 install:

```powershell
python tools/capture-pico8-arcade.py pico8/snake/snake.p8 win C:\Temp\snake-win.png
```

The capture tool modifies only a temporary copy of the cart. Release carts
always start at their title screen.

Deploy all Sprout PICO carts and catalogue artwork to an Onion card:

```powershell
powershell -ExecutionPolicy Bypass -File tools/pico8-deploy.ps1 -SdRoot G:\
```

Passing `-ProfileId` additionally creates profile-scoped cart copies with
isolated legal `cartdata()` identifiers. The managed directory stays excluded
from normal library discovery.

## Device evidence

On 2026-08-06, an Onion `v4.3.1-1` Miyoo Mini Plus received all three public
carts plus isolated `arcade-v1-dev` profile copies. Remote SHA-256 values for
the public carts matched the repository bytes. The existing Onion fake-08 core
was enabled through its on-card Package Manager payload; all three carts left a
live RetroArch process with `fake08_libretro.so` loaded. All three also left a
live `pico8_dyn` process through the separately installed licensed wrapper,
which reported an opened Miyoo audio device at 22,050 Hz.

These are packaging and process-level SSH checks. They do not prove visible
frame correctness, physical controls, perceived pacing or audio quality,
persistence interactions, clean user-driven exit, GameSwitcher return, or
cross-wrapper save compatibility. Those remain explicit hands-on device
acceptance checks. Native Sprout Runtime still has no device deploy target and
remains under the separate launcher hardware-validation halt.
