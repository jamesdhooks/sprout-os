# Roadmap

Statuses describe repository truth, not intent: **Not started**, **In progress**, **Blocked**, **Complete**, **Deferred**. No dates are assigned without delivery evidence.

## Committed near-term work

| Milestone | Status | Exit condition |
| --- | --- | --- |
| Project foundation | Complete | Public documentation, governance, initial Onion research, local coordination, and a clean Git history exist |
| [Sprout Family Launcher MVP](https://github.com/jamesdhooks/sprout-os/milestone/1) | Not started | A development SD card completes the profile-to-launch-to-return vertical slice with local policy and recovery |

The launcher MVP includes a splash; resumable offline setup; one parent and one child; built-in and imported/cropped avatars; versioned configuration; child and parent modes; local recents/favorites test data; a small discovered ROM library; one Game Boy and one SNES launch through Onion; preserved GameSwitcher behavior; clean return; daily child time limits; persistent end-of-day parent unlock; manual lock; one profile export/restore; safe-mode entry to stock Onion; and integration-test documentation.

Hardware behavior must be demonstrated on a development card. Desktop preview alone does not complete the milestone.

## Planned later

| Milestone | Status | Dependency |
| --- | --- | --- |
| Sprout Runtime MVP | Deferred | Reliable launcher vertical slice |
| First native games | Deferred | Stable runtime APIs and tests |
| Signed package catalogue and updates | Deferred | Runtime/package consumers and threat model |
| Optional sync and family server | Deferred | Stable local schemas and event journal |
| Sprout Studio | Deferred | Stable runtime and package format |
| Deeper profile-aware Onion integration | Deferred | Verified need and upstream boundary evidence |
| Raspberry Pi living-room edition | Deferred | Portable launcher/runtime behavior |

## Exploratory

Media connectors, recommendation models, browser tooling, third-party connector packages, AI-assisted curation, and broader handheld support are exploratory. They are not commitments and must not expand the launcher MVP.
