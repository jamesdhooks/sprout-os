# PICO-8 Editions

Status: **Windows PICO-8 validated; native-wrapper process launch verified on Onion; fake-08 validation pending**

Sprout keeps PICO-8 editions beside their native counterparts. They share game
rules and visual language, not runtime code or high-resolution assets. Each
cart is a standalone `.p8` file suitable for Onion's PICO/fake-08 emulator.

## Current carts

| Cart | Round completion | Failure | Persistent data |
| --- | --- | --- | --- |
| Mouse & Cheese Maze | Find the cheese and advance through the 100-level campaign | None | Current level and solved count |
| Blocks & Buttons | Seat every crate on a button | Provable corner deadlock | Completion flag |
| Starlight Snake | Collect 12 fruit | Wall or body collision | Best score |

Blocks & Buttons uses a top-down toy-workshop renderer aligned to its square
collision grid. Walls, crates, and buttons add shallow vertical faces for depth
without rotating the board into an isometric projection. Its robot uses
reviewed north, south, and west walk/push families; east mirrors west at runtime.
The west/east family is a strict screen-cardinal side view under the same high
camera, not a diagonal three-quarter pose. Every character, crate, and button
uses the bottom of its logical cell as the ground-contact baseline.
The native edition retains the 256x256 masters while the standalone cart uses
deterministically derived 16x16 PICO-8 frames. Starlight Snake uses one reviewed
head, straight, corner, and tail family. The compiler constructs one exact
connector width and rotates that geometry for every required direction, so the
7-pixel-cell PICO sprites overlap without seams or per-piece scaling. Fruit rotates through a small
visual family without changing the underlying rule.

Blocks & Buttons and Starlight Snake share the small lifecycle source in
`pico8/shared/arcade_core.lua`. `tools/pico8_arcade_build.py` injects that
source into each cartridge so deployment never depends on `#include` or an
external file. Game rules and renderers remain cart-owned.

All explicit wins use a pop-animated celebration card. When its display period
ends—or the primary action shortens it—the cart returns to its title and resets
the playable state. Failure cards remain until the player retries or returns.

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

On 2026-08-05, an Onion `v4.3.1-1` Miyoo Mini Plus accepted the reviewed Onion
PICO-8 native-wrapper package and locally supplied licensed Raspberry Pi
`pico8_dyn`/`pico8.dat` files. Mouse & Cheese Maze, Blocks & Buttons, and
Starlight Snake were transferred beneath `Roms/PICO/Sprout`; Blocks & Buttons
and Starlight Snake each started a persistent `pico8_dyn` process through the
wrapper.

The wrapper repeatedly logged `Invalid audio device ID` during both launches.
Visual output, physical input, audio behavior, clean exit, GameSwitcher return,
and fake-08 remain explicit operator/device checks rather than inferred passes.
