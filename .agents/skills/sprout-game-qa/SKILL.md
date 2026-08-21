---
name: sprout-game-qa
description: Test, review, capture, or report evidence for any Sprout native or PICO-8 game. Use for deterministic screenshot matrices, title/gameplay/win/fail/reset/pause/hint states, input and game-feel checks, persistence, procedural samples, asset readability, Windows validation, fake-08 or Miyoo acceptance, release gates, and distinguishing verified from unverified behavior.
---

# Sprout game QA

Prove externally observable behavior on every claimed target.

## Build the matrix

Read docs/development/visual-validation.md and references/evidence-matrix.md. Enumerate only real states:

- title/continue;
- representative gameplay;
- pause and return;
- success;
- each genuine failure/recovery mode;
- reset confirmation and cancellation;
- hint/help when present;
- persistence before and after relaunch;
- representative early, middle, late, and mechanic-transition content.

Do not invent a failure state for a game that cannot fail. Do not mark a state covered because code exists.

## Use deterministic hooks safely

Establish capture states through the same valid game data and renderer used in play. Keep QA hooks inert during normal launch and modify only temporary carts or capture sessions.

For native:

~~~powershell
./tools/capture-visual-validation.ps1 -OutputDirectory ../sprout-work/visual-validation
~~~

For PICO-8:

~~~powershell
python tools/pico8_game.py capture <slug> --state title
python tools/pico8_game.py capture <slug> --state gameplay
python tools/pico8_game.py capture <slug> --state win
~~~

## Review behavior, not just images

- Verify held, pressed, released, simultaneous, and blocked input.
- Check smooth movement, collision timing, animation cadence, frame pacing, and audio.
- Check B/back, pause, clean exit, replay, and destructive-reset fresh-input behavior.
- Round-trip persistence and incompatible schema handling.
- Validate deterministic seeds, golden vectors, and representative procedural bands.
- Review silhouettes, contrast, anchors, sorting, text legibility, and terminal-state clarity at native output size.

## Separate confidence levels

Report Windows native, licensed desktop PICO-8, fake-08, licensed Miyoo wrapper, and physical-device checks separately. Desktop evidence never proves Onion input, audio, suspend, GameSwitcher return, or device pacing.

Record exact commands, results, artifact paths, versions, failures, and unverified assumptions. Never fabricate a pass or silently omit a requested view.
