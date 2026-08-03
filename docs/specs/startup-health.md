# Launcher Startup Health

Status: **implemented decision, persistence, desktop launcher integration, and desktop startup presentation v1; device startup integration pending**.

The desktop launcher presents a dedicated SproutOS startup image before the
profile experience. This asset is separate from launcher backgrounds and every
game title page. The host renders the SproutOS wordmark so it remains crisp and
does not depend on lettering baked into the illustration. Onion boot-hook timing
and safe-mode interaction remain device-validation work.

## Purpose

The startup-health store gives Sprout a durable, clock-independent signal after repeated launcher failures. The desktop launcher now uses that signal to select its [configuration recovery flow](launcher-recovery.md). It does not watch a process, install a device boot hook, or prove that stock Onion can be reached.

The intended call sequence is:

1. Call `begin_startup()` before normal launcher initialization.
2. If the decision requests recovery, route to the reviewed recovery surface instead of ordinary profile navigation.
3. Call `mark_ready(attemptId)` only after required local stores are open, ordinary setup or profile state is constructed, its first interactive frame rendered successfully, and the input loop can accept actions.

Merely rendering the recovery menu is not ready. An exit, rejected recovery action, initialization failure, or process loss before step 3 deliberately leaves the attempt active.

## Decision semantics

Schema v1 uses the named threshold `kStartupRecoveryFailureThreshold = 3`.

- The first attempt reports zero prior failures.
- Beginning with an active prior attempt counts that predecessor once, replaces its marker with a new attempt ID, and saturates the failure count at three.
- Therefore, after attempts 1, 2, and 3 remain unfinished, attempt 4 reports three consecutive failures and `recoveryRequired = true`.
- A matching ready acknowledgement clears the active marker and resets the count to zero.
- A zero, missing, stale, or repeated attempt ID fails without clearing a newer marker.

The core returns a decision; callers must honor it. Calling `mark_ready` on a recovery-required attempt before a usable recovery surface exists would incorrectly erase the evidence.

## Storage and validation

`startup-health.sqlite3` is a dedicated SQLite database. Schema v1 contains one `startup_health` row with:

- `next_attempt_id`, a positive monotonically increasing integer;
- nullable `active_attempt_id`; and
- `consecutive_failures`, constrained from zero through three.

Every begin and ready transition uses an immediate transaction. The row invariants require an active ID to precede the next ID and require a zero failure count when no attempt is active. Attempt identity exhaustion fails closed.

An empty version-zero database is migrated transactionally to schema v1. A nonempty unversioned database, newer schema, missing or duplicate row, invalid type, or inconsistent state is rejected without adoption or repair. The core does not depend on wall-clock, local-date, network, or server state.

## Recovery boundary

Issue 9 remains open. The following still require source and development-card evidence:

- which physical boot action safely bypasses Sprout;
- where the Sprout startup hook can coexist with Onion's runtime;
- how the desktop-tested recovery surface is started on the device;
- whether stock Onion starts without changing ROMs, saves, states, profiles, or backups; and
- how a failed recovery surface avoids its own boot loop.

The current ARM diagnostic proves compilation and SQLite linkage only. It does not exercise the target kernel's boot lifecycle or input path.
