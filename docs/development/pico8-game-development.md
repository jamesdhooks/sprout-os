# Writing, testing, and deploying PICO-8 games

This guide is the shortest supported path from an idea to a standalone Sprout
PICO-8 cartridge. It assumes Windows and the licensed desktop PICO-8 app for
authoring. Device releases target Onion's PICO integration and must also be
checked with fake-08. If you need to learn the console, editor, P8 Lua, graphics,
map, audio, persistence, or debugging model first, read the
[PICO-8 authoring handbook](pico8-authoring-handbook.md).

Sprout never bundles the PICO-8 executable, `pico8.dat`, downloaded BBS carts,
or private save data.

## Create a game

From the repository root:

```powershell
python tools/pico8_game.py new cloud-hop --title "Cloud Hop"
python tools/pico8_game.py validate cloud-hop
python tools/pico8_game.py run cloud-hop
```

The first command creates:

```text
pico8/cloud-hop/
|-- cloud-hop.p8     Standalone source cartridge
|-- manifest.json    Stable identity, version, save policy, and runtime
`-- README.md        Game-specific rules, controls, and test notes
```

The starter cart is a tiny playable example with a title screen, 60 Hz update,
movement, a win state, B/back behavior, deterministic capture hooks, and one
profile-ready `cartdata()` identifier. Replace its game rules incrementally;
do not start by copying a large finished game.

## Edit the cartridge

A text `.p8` cartridge is deliberately inspectable. Its common sections are:

| Section | Purpose |
| --- | --- |
| `__lua__` | Game code and data written in PICO Lua |
| `__gfx__` | 128x128 sprite sheet encoded as palette indices |
| `__gff__` | Optional sprite flags |
| `__map__` | Optional tile map data |
| `__sfx__` | Sound effects |
| `__music__` | Music patterns |

PICO-8's editor is the easiest way to draw sprites, maps, SFX, and music. You
can also edit the text cart in a normal editor while PICO-8 is closed, then load
it again. Keep the release cart self-contained: Onion must not need `#include`,
source images, generators, or local paths.

Use the normal lifecycle:

```lua
function _init()
 -- initialize one run
end

function _update60()
 -- update input and simulation at 60 Hz
end

function _draw()
 -- redraw the current screen
end
```

Keep simulation state out of `_draw()`. Use deterministic seeds for generated
content and move expensive search or campaign validation into offline tools.

## Controls and Sprout conventions

PICO buttons are zero-based: left `0`, right `1`, up `2`, down `3`, A `4`, and
B `5`. Desktop PICO-8 normally maps A to `Z` and B to `X`.

- D-pad controls the primary movement or selection.
- A starts, confirms, acts, or advances.
- B returns to the title or previous game screen; it must not silently erase data.
- If reset is supported, require a deliberate hold and visible progress.
- Every terminal state needs a clear next action and a route home.
- Avoid relying on keyboard-only controls in release gameplay.

Use `btn()` for held input and `btnp()` for a new press. For smooth movement,
update position at 60 Hz rather than moving only on key-repeat events.

## Persistence

Call `cartdata()` once with the development ID in `manifest.json` before using
`dget()` or `dset()`. PICO-8 provides 64 numeric slots. Reserve slot `0` for a
small schema version, document every occupied slot in the game's README, and
migrate or reset only this cart when its schema is incompatible.

The scaffolded manifest has both:

- `devSaveId`: the desktop development slot embedded in the public cart;
- `saveTemplate`: the legal per-profile identifier Sprout materializes for a managed cart.

Never commit desktop save files or user profile carts.

## Test on Windows

Set `PICO8_EXE` if PICO-8 is not installed in its usual Windows location:

```powershell
$env:PICO8_EXE = "C:\Program Files (x86)\PICO-8\pico8.exe"
```

Then use:

```powershell
python tools/pico8_game.py validate cloud-hop
python tools/pico8_game.py run cloud-hop
python tools/pico8_game.py capture cloud-hop --state title
python tools/pico8_game.py capture cloud-hop --state gameplay
python tools/pico8_game.py capture cloud-hop --state win
```

`run` opens a 512x512 window by default; `--scale 2` through `--scale 8` changes
only the desktop window. The cart always renders at PICO-8's native 128x128.

The capture command works when the cart contains exactly one
`qa_capture="normal"` marker and handles the requested state in `_init()`.
Captures use a temporary cart, so review states never leak into the release.
Add `fail` only when the game has a real failure state.

Before calling a desktop build ready, manually check:

- title, fresh start, replay, win, and every real failure state;
- held input, simultaneous input, pause, B/back, and clean exit;
- save round-trip and incompatible-save behavior;
- representative early, middle, and late procedural content;
- readable text, sprites, and UI at an unfiltered 512x512 window;
- token, sprite-bank, map, SFX, music, and compressed-cart budgets in PICO-8.

The repository validator checks structure and Sprout metadata. The licensed
PICO-8 compiler remains the authority on cartridge and token budgets.

## Add art and audio

Draw game-resolution pixel art in PICO-8 or compile reviewed source assets into
the cart. Do not downsample arbitrary high-resolution art and assume it will be
readable. Check silhouettes, anchors, transparency colour, animation pivots,
and tile connections at native 128x128.

For Sprout's launcher, add a separate `library-cover.png` beside the cart and
set `libraryArtwork` to that filename in `manifest.json`. The cover is not the
cart title screen and is not packed into its sprite bank.

Keep each game's visual-language notes in its README or design document: palette,
perspective, outline weight, lighting direction, sprite baselines, animation
timing, and title/cover composition. That makes later assets consistent without
recording local generation services or private prompts in the public project.

## Deploy one game to Onion

With the Onion card mounted and `Roms\PICO` already present:

```powershell
python tools/pico8_game.py deploy cloud-hop --sd-root G:\
python tools/pico8_game.py deploy cloud-hop --sd-root G:\ --profile-id son
```

The first command copies the cart to
`Roms/PICO/Sprout/CloudHop/cloud-hop.p8` and updates Sprout's PICO catalogue.
If `library-cover.png` exists, it copies that too. The profile form additionally
creates a hidden managed cart under `Roms/PICO/.sprout-profiles/<hash>/` with
only its `cartdata()` ID changed. Sprout excludes that directory from normal
library discovery.

Use `tools/pico8-deploy.ps1` for a release deployment of all built-in Sprout
carts because it also runs their game-specific campaign/shared-code checks.

### Deploy over SSH

When Onion's SSH service is enabled, a mounted card is not required:

```powershell
python tools/pico8_game.py deploy-ssh cloud-hop `
  --host onion@192.168.2.72

python tools/pico8_game.py deploy-ssh cloud-hop `
  --host onion@192.168.2.72 `
  --profile-id son
```

Use `--dry-run` to print the exact remote install operations without connecting.
Use `--identity C:\path\to\key` and `--port` for a non-default SSH setup. The
tool never accepts or stores a password; OpenSSH may prompt interactively, but
an SSH key makes repeated deployment substantially easier.

The SSH path validates locally, preserves existing catalogue entries, streams
one archive into a Sprout-owned staging directory on the SD card, installs each
file through a temporary name, clears Onion's PICO catalogue cache, and calls
`sync`. It does not install PICO-8, fake-08, or the Onion PICO package itself.
The device must already expose `/mnt/SDCARD/Roms/PICO`; the default remote root
can be changed with `--remote-root`.

## Device acceptance

Desktop PICO-8 is not Miyoo evidence. Before release, check the public cart and
a Sprout-managed profile cart on the device:

- visible library title and cover;
- input mapping and held-input behavior;
- stable frame pacing and audio;
- save persistence and profile isolation;
- pause, exit, GameSwitcher return, and relaunch;
- reinstall and upgrade without unrelated save loss;
- representative content at the device's physical size.

Record the Onion version, runtime used (fake-08 or licensed wrapper), exact cart
version, and anything that remains unverified.

## Release checklist

1. Update the game README and manifest version.
2. Validate the cart and its generated/offline content.
3. Review all capture states at native resolution.
4. Load and save in licensed desktop PICO-8.
5. Test fake-08 on the target Miyoo device.
6. Verify public and per-profile launch paths.
7. Confirm clean exit and GameSwitcher return.
8. Commit only source carts, original assets, manifests, and durable docs.

Reference: [official PICO-8 manual](https://www.lexaloffle.com/dl/docs/pico-8_manual.html)
and [Onion PICO-8 documentation](https://onionui.github.io/docs/emulators/pico-8).
