# Visual validation

Sprout provides a deterministic Windows capture pass for reviewing every implemented launcher view and each native game's title, representative gameplay, and terminal presentations. Captures are review artifacts, not product assets, and remain untracked.

Run from PowerShell:

```powershell
.\tools\capture-visual-validation.ps1 -OutputDirectory ..\sprout-work\visual-validation
```

The command builds Sprout, captures the matrix, and writes `index.html` plus machine-readable `coverage.json`. Use `-SkipBuild` only after an unchanged successful build.

Capture the corresponding licensed desktop PICO-8 matrix with:

```powershell
.\tools\capture-pico8-v1.ps1 -OutputDirectory ..\sprout-work\pico8-validation
```

`tools/build_arcade_contact_sheets.py` builds native-size and enlarged local
review sheets from either capture tree. Contact sheets and captures remain
untracked evidence rather than product assets.

## Deterministic game states

The runtime accepts `--capture-state <name>` only together with `--capture`. It invokes a package's capture-only `capture_scenario(name)` hook after normal initialization and before the first frame. The hook is never called during ordinary play and must establish a valid state using the same data rendered by the game.

| Game | Representative gameplay | Success | Failure | Special state |
| --- | --- | --- | --- | --- |
| Starlight Snake | Round and Endless | Round complete | Collision | Mode selection and speed stage |
| Mouse & Cheese Maze | Early, middle, and late density | Cheese reached | Not applicable | Hint path |
| Blocks & Buttons | Early, middle, and late rooms | Room and campaign completion | Dead square | One-step undo |

Every native and PICO edition also captures title reset idle, holding,
cancelled, and complete states. A reset capture is not accepted unless the
underlying fresh-input and one-shot persistence tests pass.

Blocks & Buttons treats an off-button crate trapped against perpendicular walls as a provable deadlock. The calm retry view freezes movement and the primary action restarts the room.

The runtime does not yet render a pause overlay. Pause and resume lifecycle behavior is tested independently, and `coverage.json` records the missing presentation explicitly.

The launcher matrix also covers startup, setup, recovery, profile selection,
profile image pages, the profile appearance chooser, the background catalogue,
parent/child homes, libraries, and archive flows. Capture mode freezes ambient
focus animation at a deterministic phase; ordinary profile selection uses a
gentle bob, scale, and rotation treatment.

## Review standard

Review the generated index at its native 640 x 480 output and check cropping, legibility, focus state, asset loading, consistent terminology, and clear success or recovery actions. These captures verify the Windows renderer only; they do not replace Miyoo Mini Plus device-card validation.
