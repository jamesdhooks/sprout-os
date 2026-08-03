# Runtime Game Lifecycle

Status: **Target contract; v1 supports only start/update/render/stop and test snapshots**

The host owns lifecycle and policy. A game can request a return, but it cannot terminate the process, read wall-clock time, extend play time, or bypass a host pause.

## Session sequence

```text
validate package and assets
        ↓
create deterministic context
        ↓
new game init ─────── or ─────── restore checkpoint
        ↓
fixed update → render commands → fixed update
        ↓                    ↘
host pause/suspend ───────→ checkpoint
        ↓
resume or host-directed exit
        ↓
final events and durable progress flush
```

The host may pause between ticks. Rendering is optional while paused and never mutates simulation state.

## Input frame

Each fixed update receives action state, not platform keys:

```text
held:      up down left right primary secondary start
pressed:   edge for each action
released:  edge for each action
frame:     deterministic session frame number
```

The host reserves secondary/back for return behavior. A game receives a return request so it can present or flush game state, but the host can still enforce exit after its bounded checkpoint window.

## Seed and difficulty context

New sessions receive a base seed, mode/content identity, and an explicit difficulty record. Games request named RNG streams such as `level`, `spawn`, or `effects`. Stream state is part of the checkpoint.

Campaign content includes resolved layout plus generator provenance. Endless/daily content stores generator ID/version, seed, difficulty parameters, and measured metrics. Games never derive difficulty from profile age directly; the launcher/policy layer selects an allowed difficulty record.

## Checkpoints and durable records

A checkpoint is an atomic versioned document bounded by the runtime. It contains runtime frame state plus a game-owned schema payload. Restore must either migrate/accept the whole checkpoint or fail safely into a new session; partial restore is forbidden.

Durable records are separate from resumable checkpoints:

- campaign progress and completed content IDs;
- per-mode high scores;
- achievements already emitted;
- per-profile difficulty/progression state; and
- settings allowed by the package schema.

The runtime writes profile/package-isolated data atomically and retains the last-known-good checkpoint. Game scripts do not encode files or choose paths.

## Events

The runtime owns `GameStarted`, pause/resume, playtime, and `GameExited`. Games may emit validated achievement, level-completion, score/round, and return-request facts. Event payload schemas are versioned and bounded; free-form text is not a substitute for structured fields.

Events are facts, not commands. An `AchievementUnlocked` event cannot unlock parent access, extend time, or alter package permissions.

## Error and recovery behavior

- Package, asset, content, or checkpoint validation fails before play with a recoverable launcher message.
- A lifecycle error stops the game session, preserves the last-known-good checkpoint, and returns control to the launcher.
- Missing optional presentation assets may use an engine-owned fallback only when the package declares and tests that fallback.
- Content generator version mismatches do not regenerate shipped campaign layouts.
- Unverified platform suspend behavior remains explicitly unclaimed until tested on that platform.
