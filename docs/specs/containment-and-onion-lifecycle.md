# Sprout Containment and Onion Lifecycle Contract

Status: implementation in progress; Card A hardware acceptance required.

## Objective

While Sprout containment is enabled, every ordinary boot, app return, game exit,
GameSwitcher exit, and launcher restart must resolve to Sprout. Stock Onion MainUI
is available only after explicit parent authorization or an administrator recovery
action that cannot be triggered with handheld buttons alone.

## Trust boundaries

- Onion `runtime.sh` remains the owner of emulator launch, Activity Tracker,
  service suspension/restoration, save/exit handling, power handling, and
  GameSwitcher.
- Sprout owns profile selection, child policy, parent authentication, library
  authorization, and the decision to hand a validated game request to Onion.
- Onion `keymon` remains global. In Apps mode it does not act on a single MENU
  press. The Miyoo SDL backend maps the center MENU button to `SDLK_HOME`;
  Sprout atomically requests Onion's GameSwitcher and returns control to the
  Onion runtime. SELECT (`SDLK_ESCAPE`) remains the contained administrative
  `SystemMenu` action.
- MainUI is closed source and is not modified.

## Launcher input and authorization

1. `GameSwitcher`/MENU, `SystemMenu`/SELECT, `Back`/B, and the primary
   `Menu`/START action are distinct.
2. At profile selection or under a child profile, `SystemMenu` opens masked
   parent-PIN verification.
3. A correct PIN authorizes exactly one exit to stock Onion for the current boot
   session. A wrong or cancelled PIN remains in Sprout.
4. An actively selected parent profile with a current grant may exit directly.
   A stale grant, an inactive parent, or profile selection alone does not.
5. `Back` at profile selection is inert.
6. In contained-device mode unsolicited SDL quit events are ignored. Process
   failure is handled by the supervisor contract below, not interpreted as an
   authorized exit.
7. Setup and launcher recovery remain inside Sprout. They cannot expose MainUI
   through MENU.

## Boot and supervision

A narrow, version/hash-guarded patch to Onion `runtime.sh` changes only two idle
transitions:

- `check_main_ui`: when containment is enabled and neither the boot-session
  parent-exit marker nor the administrator maintenance flag is present, queue
  the Sprout App command instead of launching MainUI.
- `check_switcher`: when Sprout has atomically staged a validated game command
  and its handoff marker, preserve that command for Onion's next `check_game`
  pass rather than deleting it.

Sprout's App wrapper supervises the launcher. It restarts clean or abnormal
unapproved exits, allowing the existing three-unfinished-start recovery UI to
appear. It propagates the dedicated game-handoff status to Onion. Parent-approved
exit creates a `/tmp` boot-session marker; it does not permanently disable
containment.

Holding MENU during boot may still clear Onion's stale auto-resume game command,
but the subsequent idle transition routes to Sprout. It is not a containment
bypass.

## Game and GameSwitcher lifecycle

1. Sprout validates profile visibility, launch policy, ROM containment,
   extension, launcher path, and child-time availability.
2. It atomically writes Onion's `cmd_to_run.sh`, then creates a handoff marker,
   and exits with a dedicated status.
3. Onion consumes the command through its unchanged `launch_game` path. This
   preserves Activity Tracker, service management, per-game core overrides,
   save UI, power checks, and error reporting.
4. Global MENU behavior during a game remains Onion-owned. GameSwitcher may
   pause/resume or select another game through Onion's existing command path.
5. When the game/GameSwitcher chain ends and no next game command exists,
   Onion's idle transition launches Sprout rather than MainUI.
6. Sprout must restore the active profile and library context after return.
7. Child play-time accounting must eventually consume lifecycle evidence across
   play, pause, GameSwitcher, suspend, and return; until this is wired and proven,
   child emulator launch is not a completed release gate.

## Recovery and rollback

There is deliberately no button-only stock-Onion chord: that would contradict
containment against arbitrary child button presses.

Administrator recovery is provided by either:

- parent-authorized exit from a working Sprout session; or
- a documented SD/SSH maintenance flag checked before idle routing, used when
  Sprout cannot start.

Runtime integration installation must:

1. verify the exact supported Onion runtime hash;
2. preserve a timestamped byte-for-byte backup;
3. stage and syntax-check the patched script;
4. activate atomically;
5. read back and hash both active and backup files; and
6. provide a hash-guarded rollback that does not touch ROMs, saves, states,
   profiles, MainUI, or unrelated Onion configuration.

## Required hardware acceptance

- Cold boot and warm reboot route directly to Sprout.
- Held MENU at boot cannot expose MainUI.
- Every face/shoulder/START/SELECT/MENU combination at profile selection and in
  a child session remains contained.
- Child MENU prompts for PIN; wrong/cancel stays contained; correct PIN reaches
  MainUI once.
- Active authenticated parent MENU reaches MainUI; reboot restores containment.
- Normal Back at profile root is inert.
- GB, SNES, PICO-8/fake-08, RetroArch, and each supported standalone emulator
  return to Sprout on normal exit and abnormal exit.
- GameSwitcher opens, resumes the current game, switches games, and exits back
  to Sprout without a MainUI path.
- Saves/states, Activity Tracker, recents/GameSwitcher history, and profile
  separation remain correct.
- Suspend/resume, low-battery shutdown, power cycle, interrupted handoff, and
  three deliberately unfinished launcher starts fail closed.
- Parent grant expiry and clock rollback fail closed at launch and resume.
- SD/SSH maintenance recovery and runtime rollback both restore stock Onion
  without modifying user data.

Host tests, ARM audit, package-contract checks, staged deployment, active-file
hash readback, and direct Card A observations are all required. Source inspection
or a successful cross-build alone cannot satisfy a hardware row.
