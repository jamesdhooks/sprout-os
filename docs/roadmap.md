# Roadmap

Statuses describe repository truth, not intent: **Not started**, **In progress**, **Blocked**, **Complete**, **Deferred**. No dates are assigned without delivery evidence.

## Committed near-term work

| Milestone | Status | Exit condition |
| --- | --- | --- |
| Project foundation | Complete | Public documentation, governance, initial Onion research, local coordination, and a clean Git history exist |
| [Sprout Family Launcher MVP](https://github.com/jamesdhooks/sprout-os/milestone/1) | Blocked | A development SD card completes the profile-to-launch-to-return vertical slice with local policy and recovery |
| [Sprout Arcade Windows Preview](https://github.com/jamesdhooks/sprout-os/milestone/2) | In progress | The launcher discovers and starts one packaged native microgame through a deterministic Windows runtime, with automated package, lifecycle, and integration tests |

The launcher MVP includes a splash; resumable offline setup; one parent and one child; built-in and imported/cropped avatars; versioned configuration; child and parent modes; local recents/favorites test data; a small discovered ROM library; one Game Boy and one SNES launch through Onion; preserved GameSwitcher behavior; clean return; daily child time limits; persistent end-of-day parent unlock; manual lock; one profile export/restore; safe-mode entry to stock Onion; and integration-test documentation.

Hardware behavior must be demonstrated on a development card. Desktop preview alone does not complete the launcher milestone. Its remaining device issues are deliberately deferred until suitable Onion hardware and a development card are available; they remain open and are not treated as complete.

The exact pre-Arcade stopping point and hardware resume procedure are recorded in [Launcher MVP Hardware Halt](development/launcher-mvp-halt.md).

The Windows Arcade preview is a separate evidence track. It now establishes contracts exercised by a real local package and game: deterministic stepping, action input, rendering, profile-isolated local storage, structured events, strict package discovery, audience-based child visibility, launcher handoff, and clean process return. Live time-limit enforcement during an external game remains open. The milestone does not include a remote catalogue, downloads, signing infrastructure, publishing, arbitrary third-party code, or device compatibility claims.

## Planned later

| Milestone | Status | Dependency |
| --- | --- | --- |
| Portable Sprout Runtime beyond Windows | Deferred | Stable Windows runtime APIs and a real packaged game |
| First native game collection | Deferred | Windows preview proves one complete game and package lifecycle |
| Signed package catalogue and updates | Deferred | Runtime/package consumers and threat model |
| Optional sync and family server | Deferred | Stable local schemas and event journal |
| Sprout Studio | Deferred | Stable runtime and package format |
| Deeper profile-aware Onion integration | Deferred | Verified need and upstream boundary evidence |
| Raspberry Pi living-room edition | Deferred | Portable launcher/runtime behavior |

## Exploratory

Media connectors, recommendation models, browser tooling, third-party connector packages, AI-assisted curation, and broader handheld support are exploratory. They are not commitments and must not expand the launcher MVP.
