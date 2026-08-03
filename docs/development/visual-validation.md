# Visual validation

Sprout provides a deterministic Windows capture pass for reviewing every implemented launcher view and each native game's title, representative gameplay, and terminal presentations. Captures are review artifacts, not product assets, and remain untracked.

Run from PowerShell:

```powershell
.\tools\capture-visual-validation.ps1 -OutputDirectory ..\sprout-work\visual-validation
```

The command builds Sprout, captures the matrix, and writes `index.html` plus machine-readable `coverage.json`. Use `-SkipBuild` only after an unchanged successful build.

## Deterministic game states

The runtime accepts `--capture-state <name>` only together with `--capture`. It invokes a package's capture-only `capture_scenario(name)` hook after normal initialization and before the first frame. The hook is never called during ordinary play and must establish a valid state using the same data rendered by the game.

| Game | Title | Gameplay | Win | Fail |
| --- | --- | --- | --- | --- |
| Sprout Snake | Captured | Captured | Not applicable: endless game | Captured: collision |
| Mouse & Cheese Maze | Captured | Captured | Captured: cheese reached | Not applicable: no loss condition |
| Blocks & Buttons | Captured | Captured | Captured: every crate placed | Captured: provable deadlock |

Blocks & Buttons treats an off-button crate trapped against perpendicular walls as a provable deadlock. The calm retry view freezes movement and the primary action restarts the room.

The runtime does not yet render a pause overlay. Pause and resume lifecycle behavior is tested independently, and `coverage.json` records the missing presentation explicitly.

## Review standard

Review the generated index at its native 640 x 480 output and check cropping, legibility, focus state, asset loading, consistent terminology, and clear success or recovery actions. These captures verify the Windows renderer only; they do not replace Miyoo Mini Plus device-card validation.
