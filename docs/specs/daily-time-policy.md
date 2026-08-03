# Daily Time Policy

Status: **implemented storage and decision core v1; device lifecycle integration pending**.

## Scope

The v1 core enforces one profile-scoped daily allowance from 1 second through 24 hours. It persists policy, daily usage, warning acknowledgement, clock observations, and at most one active device session in SQLite. It does not implement session maximums, breaks, schedules, category budgets, remote extensions, or server time authority.

The core consumes explicit start/resume, checkpoint, pause, and exit-equivalent calls. It does not infer process, screen, suspend, GameSwitcher, or power state. Issue-level completion still requires a hardware-verified lifecycle adapter to supply those calls and perform Onion's normal save-and-exit path.

## Accounting contract

- Live active usage is the difference between caller-supplied monotonic millisecond samples.
- Launcher time, pause, suspend, and powered-off time consume nothing when their lifecycle boundaries are reported correctly.
- Checkpoints update daily usage and the active marker transactionally. Device integration must checkpoint at least every 30 seconds while a game is active.
- A monotonic rollback, wall-clock rollback, or local-date rollback is rejected. Callers must fail closed rather than permit a launch or resume after that error.
- A forward local-date transition during active play charges the observed monotonic delta to the prior day, clears the active marker, and requests normal save-and-exit. A subsequent authorized start uses the new day's allowance.

The wall clock selects the local usage bucket and detects rollback; it does not measure a live session.

## Restart recovery

An unclosed active marker means the prior process or device stopped between lifecycle boundaries. On restart, recovery charges the positive wall-clock gap since the last checkpoint, capped at 30 seconds, then clears the marker. If the local date changed, recovery charges the full 30-second bound to the prior day and requests a normal return before another start. A backward clock is rejected and leaves the marker intact so recovery cannot silently authorize play.

This bounds lost accounting without charging an unbounded powered-off interval. It depends on the integration honoring the 30-second checkpoint maximum.

## Decisions and warnings

Each operation returns the allowance, used time, remaining time, and expiration state. Start/resume is blocked when remaining time is zero. Reaching zero while active requests normal save-and-exit; the core does not kill a process.

The core emits notices once per profile and local date when remaining time crosses 10, 5, and 1 minutes. A persisted bit mask prevents duplicate warnings across pause, resume, and process restart. Presentation and any in-game overlay remain separate consumers.

## Storage and migration

Schema version 1 contains:

- `daily_policy`: one allowance per profile;
- `daily_usage`: used milliseconds and warning state per profile/date;
- `profile_clock`: last accepted local date and UTC observation per profile; and
- `active_session`: the single device-active session checkpoint.

A newer schema is rejected without modification. Policy rows reference stable profile IDs but remain in a separate database, so integration must configure or remove policy alongside profile lifecycle operations.

## Hardware acceptance still required

Tests deterministically cover persistence, active-only accounting, warning crossings, expiration, interrupted recovery, clock/date rollback, midnight transition, invalid input, and newer schemas. They do not prove Onion process observation, GameSwitcher resume interception, suspend detection, save-state completion, return routing, checkpoint durability on the SD card, or device clock behavior.
