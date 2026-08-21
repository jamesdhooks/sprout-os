# Arcade Windows Evidence

Status: **Automated lifecycle evidence implemented; manual checklist required for release claims**

This protocol verifies local Sprout Arcade behavior on Windows without implying Linux, browser, Raspberry Pi, Miyoo, Onion, GameSwitcher, safe-boot, or device-performance support.

## Automated command

From a Windows checkout with the required C++ tools:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\dev.ps1 -Action test
```

The command configures and builds the launcher/runtime, runs every CTest target, launches the checked-in Snake package through the real launcher adapter, then launches a reviewed lifecycle fixture through the same boundary.

Required output includes:

```text
100% tests passed
SPROUT_EVENT GameStarted fresh
SPROUT_EVENT GameExited normal
arcade smoke result: arcade:sprout.snake outcome=0 exit=0
SPROUT_EVENT AchievementUnlocked lifecycle-smoke
arcade smoke result: arcade:sprout.lifecycle-fixture outcome=0 exit=0
```

Whitespace between event fields is tab-delimited in actual output.

## Automated evidence matrix

| Boundary | Evidence |
| --- | --- |
| Package discovery | Strict scanner tests plus explicit lookup of `arcade:sprout.snake` and the lifecycle fixture |
| Invalid packages | Scanner tests skip malformed packages and reject duplicate identities without crashing |
| Typed launch/policy | Native adapter tests cover fixed executable, structured arguments, policy denial, identity change, path validation, and process outcomes |
| Real process start | Launcher smoke starts the built `sprout-runtime.exe` rather than a recording process |
| Play event | Runtime emits `GameStarted fresh`; the fixture emits validated `AchievementUnlocked lifecycle-smoke` |
| Profile storage | Fixture writes `smoke-written: 1` through runtime storage; the script asserts the profile/package-isolated JSON path and value |
| Exit and return | Runtime emits `GameExited normal`; launcher reports completed outcome and exit code 0 |
| CI | The Windows Actions job executes the same `tools/dev.ps1 -Action test` command |

The fixture lives under `launcher/tests/fixtures/arcade/` and is never scanned by the ordinary launcher `games/` root. It contains no external assets or dependencies and is covered by the repository license.

## Dependency and license review

- This evidence slice adds no build or runtime dependency.
- The runtime continues to use the pinned Lua source and retained notice described in [Dependency Decisions](dependencies.md).
- Snake retains its package `LICENSE`; its current presentation uses original project artwork and the shared runtime renderer.
- The lifecycle fixture is repository test code, carries no bundled media, and is not catalogue content.
- Future accepted game assets must pass the [Arcade asset provenance workflow](../arcade/assets.md) before entering a package.

## Manual keyboard and controller check

Record the commit, Windows version, build command, input device, profile, package ID/version, seed or mode, and any capture paths. Use only sanitized ignored preview data.

1. Run `tools/dev.ps1 -Action run` and complete or resume local setup.
2. Activate a child profile and open **Sprout Arcade**.
3. Confirm Snake is visible because its manifest audience is `family`.
4. Launch it and confirm the launcher window yields focus to the game.
5. With keyboard, steer using the arrow keys or WASD, reach at least one fruit, collide, restart with primary action, and exit with Escape.
6. Repeat with an SDL-compatible controller using D-pad, A, and the controller Back button.
7. Confirm the launcher returns to the Arcade view and remains responsive after each exit.
8. Exhaust or configure zero child time in sanitized data and confirm launch is denied before the runtime process starts.
9. Activate a parent profile and confirm the same package launches without child-time policy.

If no controller is available, mark controller evidence **Not run**; keyboard success is not a substitute.

### Mouse Maze QA level jumps

Mouse Maze includes edge-triggered development chords for inspecting density
bands without completing every generated maze:

| Jump | Action chord | Windows keyboard |
| --- | --- | --- |
| +1 | Start + A + D-pad Right | Enter + Right |
| +10 | Start + A + D-pad Up | Enter + Up |
| +100 | Start + A + D-pad Down | Enter + Down |

Holding a chord triggers once; release it before another jump. Jumps persist the
new current level and cap at the edition's campaign boundary. The
game consumes logical actions, so the same chord is portable to the Miyoo input
adapter, but physical-device mapping remains unverified until dev-card testing.

## Visual check

- Launcher Arcade view is readable at its 640×480 preview size.
- Runtime content uses each package's declared surface; the current native
  collection targets the full 640x480 handheld output.
- Title, score, best score, board, fruit, snake, and round-over state are readable without clipping.
- Focus changes do not expose a stale or frozen launcher frame over the game.
- Returning from the runtime restores the launcher at the expected size and focus.

Use renderer-owned BMP capture routes for deterministic presentation evidence where available. A desktop screenshot can supplement, but not replace, runtime-owned output.

## Evidence record

```text
Commit:
Windows/build tools:
Command:
Automated result:
Keyboard result:
Controller result:
Visual result:
Profile/package/seed:
Capture/log paths:
Known limitations:
```

Do not commit household data, credentials, exported profiles, ROM names, private paths, or raw storage copied from a real family profile.

## Remaining boundaries

The smoke session is noninteractive and proves process lifecycle, events, storage, and return. The manual check proves current Windows input and presentation. Neither establishes forced time-limit exit during a running external process, platform suspend/resume, signed package trust, hostile-package isolation, or any target-device behavior.
