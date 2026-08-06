---
name: sprout-game-design
description: Define or revise a Sprout Arcade game, shared game concept, native edition, or PICO-8 edition before implementation. Use for gameplay loops, controls, state flow, difficulty, progression, audience, feature scope, edition selection, acceptance criteria, game-feel requirements, and shared-versus-platform-specific design decisions.
---

# Sprout game design

Establish the smallest complete game contract before changing runtime or cart code.

## Ground the design

1. Read `docs/arcade/README.md`, `docs/arcade/first-three-games.md`, and the relevant existing game files.
2. Read `docs/runtime/README.md` for native work or `docs/development/pico8-game-development.md` for PICO-8 work.
3. Read `references/edition-selection.md` and classify the requested output as native, PICO-8, or a paired edition.
4. Treat `game-design/<slug>/` as the shared source of truth when both editions exist. Share rules and campaign concepts, never runtime code or rendered assets.

## Define the contract

Record:

- intended player and session length;
- one-sentence objective and repeatable core loop;
- action-level controls, including back, pause, reset, and accessibility behavior;
- title, play, pause, success, failure, recovery, and return transitions;
- deterministic seed/content identity where applicable;
- progression, difficulty bands, persistence, and profile isolation;
- observable acceptance states and representative content samples;
- explicit non-goals for the current vertical slice.

Require a playable beginning, middle, terminal state, replay path, and clean route home. Do not use UI, effects, content volume, or procedural variety to conceal an incomplete loop.

## Separate shared and edition-specific decisions

Keep shared:

- rules, scoring, campaign bands, level metrics, naming, visual-language essence, and acceptance intent.

Keep edition-specific:

- renderer, input API, persistence API, asset resolution, animation budget, UI composition, audio implementation, and performance budget.

Do not force native fidelity into PICO-8 or constrain native games to PICO-8's grid, palette, or code budget.

## Control scope

- Add only mechanics exercised by the current milestone.
- Prefer reversible content/data decisions over new engine abstractions.
- Require two concrete native consumers before extracting a generic runtime helper unless the code crosses a platform or security boundary.
- Mark device behavior unverified until tested on Miyoo.
- Never silently invent age policy, parental-control behavior, dates, campaign length, or hardware capability.

## Route the work

- Use `$sprout-native-game` for native implementation.
- Use `$sprout-pico8-game` for cartridge implementation.
- Use `$sprout-procedural-content` for seeded or offline-generated content.
- Use `$sprout-game-assets` for visual-language and asset production.
- Use `$sprout-game-qa` before claiming completion.
- Use `$sprout-game-deploy` for Windows, SD-card, or SSH delivery.

Finish with explicit acceptance criteria that can be demonstrated in captures or automated tests.
