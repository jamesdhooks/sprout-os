---
name: sprout-procedural-content
description: Design, implement, generate, solve, score, deduplicate, preview, or validate deterministic procedural content for any Sprout native or PICO-8 game. Use for campaign generation, seeds, difficulty bands, offline level search, solvers, golden vectors, metrics, accepted-content exports, runtime seed payloads, or generator-version migrations.
---

# Sprout procedural content

Make generated content reproducible, measurable, visually reviewable, and edition-aware.

## Establish the content contract

1. Read docs/arcade/content-pipeline.md and the game's design/campaign specification.
2. Read references/campaign-contract.md.
3. Record game ID, generator ID, generator version, seed format, parameter bands, structural invariants, solver requirements, metrics, rejection rules, deduplication signature, and export schema.
4. Derive campaign length and band boundaries from the current game specification. Never reuse an old 100- or 1,000-level assumption silently.

## Separate offline and runtime work

Perform broad candidate search, solving, metric analysis, deduplication, acceptance, and campaign assembly offline. Ship accepted resolved layouts when practical.

Generate on-device only when the runtime algorithm is bounded and the cart/package receives an approved seed or compact parameter record. Never run campaign search or acceptance analysis during play.

## Build deterministic content

- Serialize seeds losslessly, using decimal strings in shared JSON.
- Increment generatorVersion when algorithm output changes.
- Preserve resolved old campaign content across generator upgrades.
- Use game-specific metrics; do not invent a universal difficulty score.
- Keep cosmetic randomness in a separate named stream from topology and rules.
- Add golden vectors spanning the first, middle, last, and each mechanic transition.

## Gate acceptance

For every candidate:

1. Validate bounds and structural invariants.
2. Solve or prove reachability when applicable.
3. Measure route, branching, dead-end, action-count, risk, and readability metrics relevant to the game.
4. Reject impossible, degenerate, trivial, visually poor, or UI-conflicting layouts.
5. Deduplicate exact and near-equivalent content.
6. Assign explicit mechanics and broad difficulty bands.
7. Interleave accepted content to avoid repetitive runs.

Numerical metrics never replace representative visual and play review.

## Validate both editions

When native and PICO-8 editions share a campaign concept, require equivalent seed/version semantics and edition-specific layout hashes or golden vectors. Do not assume the two implementations share PRNG behavior, dimensions, assets, or runtime code.

Export only accepted records and runtime indexes. Keep candidate databases, solver traces, rejection dumps, and generation diaries ignored.
