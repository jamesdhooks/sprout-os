# Arcade Content Pipeline

Status: **Planned for Mouse Maze and Blocks & Buttons**

Procedural content must be reproducible, measurable, and reviewable. Runtime generation is appropriate for endless/daily content; large campaigns are generated, solved, scored, deduplicated, and exported offline.

## One focused tool

Begin with one `sprout-content` developer tool containing concrete subcommands rather than six empty executables:

```text
sprout-content generate GAME
sprout-content solve GAME
sprout-content score GAME
sprout-content dedupe GAME
sprout-content build-campaign GAME
sprout-content preview GAME
sprout-content validate GAME
```

Game-specific generators and solvers implement a small tool-side interface. They are not runtime engine modules and are added only with their game consumer.

## Determinism contract

A generated item is identified by game ID, generator ID, generator version, and unsigned 64-bit seed. Identical inputs on a supported tool version must produce byte-equivalent resolved content and metrics. Algorithm changes increment `generatorVersion`; old campaign content remains loadable because resolved layouts ship with the package.

```json
{
  "schemaVersion": 1,
  "gameId": "sprout.mouse-maze",
  "contentId": "campaign-0001",
  "generator": "mouse-maze-dfs",
  "generatorVersion": 1,
  "seed": "123456",
  "difficulty": {
    "score": 0.08,
    "band": 1,
    "ageSuitability": "young-child",
    "mechanics": ["move", "reach-goal"],
    "metrics": {}
  },
  "layout": {}
}
```

Seeds are serialized as decimal strings to avoid precision loss in JSON consumers.

## Scratch database and exports

Batch generation may use an ignored SQLite database containing candidates, layouts, solutions, metrics, signatures, campaign assignments, and rejection reasons. It is reproducible scratch state and is not committed.

Tracked package content contains only:

- versioned campaign metadata;
- accepted resolved layouts;
- seed and generator provenance;
- measured metrics needed by difficulty/profile systems; and
- compact indexes required for runtime lookup.

Raw candidate dumps, solver traces, rejected layouts, and generation diaries remain untracked.

## Campaign build gates

1. Generate a broad candidate pool from explicit parameter bands.
2. Validate structural invariants.
3. Solve when the game has a solvability condition.
4. Measure game-specific metrics; do not substitute one universal difficulty formula.
5. Reject degenerate, visually poor, impossible, or trivial content.
6. Deduplicate exact and near-equivalent layouts using game-specific signatures.
7. Partition by mechanics and broad difficulty bands.
8. Interleave to avoid long repetitive runs.
9. Export resolved content deterministically.
10. Replay every export through the runtime-side content loader in automated tests.

Campaign review samples the start/end of every band and every mechanic transition. A numerical score cannot replace visual and play review.

## First consumers

- Mouse Maze: deterministic DFS/loop generator, BFS analysis, readability filter, campaign builder.
- Blocks & Buttons: reverse generator, forward solver, deadlock analysis, solution signature, campaign builder.
- Snake: no offline campaign; the same seed record and difficulty schema describe endless/daily runs.
