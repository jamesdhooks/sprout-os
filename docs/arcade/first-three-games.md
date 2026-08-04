# First Three Arcade Games

These games are the committed consumers for the reusable engine foundation. “Complete” means the package, engine features, content, assets, progression, persistence, tests, and Windows play evidence below all exist; a runnable mechanic alone is still a prototype.

## 1. Mouse & Cheese Maze

### Player experience

- The whole maze remains visible and fills the 320×240 logical canvas, apart
  from narrow symmetric side gutters required by the odd-cell maze grid.
- D-pad moves the mouse one tile per press; held input interpolates continuously
  between tile centers at a fixed cadence without an initial repeat pause.
- Reaching the cheese completes the level with a short readable celebration,
  then advances automatically. Primary action may shorten—but not eliminate—the
  celebration; secondary action asks the host to return.
- There are no enemies, lives, timers, move limits, or required hints.

### Content and progression

Use seeded randomized depth-first generation with optional loop insertion. BFS selects a cheese cell within the requested route-distance band. Generation records route length, board dimensions, branches, junctions, dead ends, wrong-branch depth, loop count, generator version, and seed.

The Windows package currently implements deterministic depth-first mazes,
farthest-reachable BFS goal placement, a persisted campaign seed and current
level, automatic next-level flow, and progressive board-density bands. Each
band chooses the largest tile size that fits its odd-dimension grid plus a
one-cell perimeter within the entire 320x240 play area. That perimeter uses a
quiet forest-ground color, separates playable tiles from the display edge, and
holds a centered level tab exactly one active cell tall. The tab scales with
each density band while retaining a minimum readable width and expanding for
multi-digit level numbers. Early levels
therefore use very few large,
richly resampled cells; later levels add cells until reaching the densest grid.
Mouse and goal artwork scale as a proportion of the current cell and remain
centered on it. Mouse coverage is calculated from its body silhouette rather
than its tail-inclusive extent or transparent 256px atlas cell. Its complete
head-to-rump silhouette remains within the cell while only the tail may cross a
boundary. Rendering uses a
floor pass, actor pass, and masked wall pass so crossed walls correctly occlude
the mouse. Both actors visibly grow with early-level tiles and shrink with later
density bands.

The padded frame is horizontally centered and top-aligned. Because square
cells and full-map visibility are preserved, a band's padded aspect ratio may
leave a narrow remainder on one axis; vertical remainder is placed below the
maze rather than above the level tab. Cells are never stretched to conceal it.

| Levels | Maze grid | Logical tile | Padded frame |
| --- | --- | --- | --- |
| 1 | 5x3 | 45px | 315x225 |
| 2-3 | 7x5 | 34px | 306x238 |
| 4-6 | 9x7 | 26px | 286x234 |
| 7-10 | 11x9 | 21px | 273x231 |
| 11-15 | 13x9 | 21px | 315x231 |
| 16-24 | 15x11 | 18px | 306x234 |
| 25-39 | 17x13 | 16px | 304x240 |
| 40+ | 19x15 | 14px | 294x238 |

The curated 1,000-layout campaign, loop insertion, and exported metrics remain
planned content-pipeline work.

The target campaign contains 1,000 resolved layouts with seed provenance.
Early levels use large tiles and short routes; later bands increase board size
and navigation complexity while preserving legibility. Daily and endless modes
will generate from a supplied seed using the same versioned generator.

A hidden, edge-triggered QA chord can jump by 1, 10, or 100 levels and caps at
the target campaign boundary. It uses only portable runtime actions; it is a
development inspection aid, not player progression.

### Completion evidence

- The generator is deterministic and connectivity is proven.
- Campaign export contains 1,000 unique accepted layouts with a monotonic broad difficulty curve and no unreadable tile size.
- Every resolved layout is replayed by an automated validator.
- Progress, current level, completed levels, and selected mode resume per profile.
- At least three achievements exercise structured events without being required for progression.
- Windows keyboard and controller checks cover movement, completion, next level, restart after resume, and return.

## 2. Blocks & Buttons

### Player experience

- D-pad moves the player and pushes one crate when the destination is free.
- All crates on buttons completes the room.
- A provably deadlocked state shows a calm retry state; primary action restarts instantly.
- Secondary action asks the host to return.
- V1 has no undo and no move limit.

### Content and progression

Generate offline from solved states using legal reverse pulls, then solve forward. Reject unsolvable, trivial, duplicate, visually poor, or misleadingly equivalent rooms. Store resolved layouts and solution-derived metrics; the runtime never runs the full campaign solver.

The shipped campaign contains 1,000 curated rooms. Early levels use one crate, middle bands introduce two, and later bands use at most three. Difficulty parameters include minimum pushes, direction changes, crate-order dependencies, player repositioning, plausible push count, deadlock temptations, and board dimensions.

### Completion evidence

- The offline solver proves every shipped room and records minimum pushes.
- Static and dynamic deadlock checks agree with solver fixtures.
- Campaign export contains 1,000 unique rooms with solution/signature deduplication and reviewed progression samples.
- Runtime state resumes player, crates, room, move/push counts, and completion state per profile.
- Automated playthrough fixtures cover legal pushes, blocked pushes, win, deadlock, retry, save, and restore.
- Windows keyboard and controller checks cover the complete room lifecycle and return.

## 3. Snake

### Player experience

- D-pad queues a legal direction; the snake advances at fixed deterministic intervals.
- Fruit always spawns on a free cell.
- Wall or body collision ends the run; primary action restarts immediately.
- Secondary action asks the host to return.
- The board, score, seed/mode label, and end state remain readable at 320×240.

### Modes and progression

V1 includes standard endless mode and a seeded daily run. The difficulty curve shortens the movement interval after configured fruit thresholds and records the parameters used for each run. A relaxed wraparound variant and obstacle mode remain later additions unless testing shows they are needed for accessibility.

High scores are stored per profile and mode. A resumable run stores the seed, RNG stream state, snake cells, direction queue, fruit, score, tick phase, difficulty stage, and terminal state. Replaying the same seed and action frames must yield the same snapshots and fruit sequence.

### Completion evidence

- Shared engine text, sprite/tile rendering, input edges, state flow, storage, and checkpoint APIs replace the one-off equivalents in the prototype.
- Tests cover deterministic fruit sequences, full-board behavior, queued turns, self/wall collision, speed thresholds, daily seed replay, high scores, save/restore, achievements, and return.
- Minimal original pixel art replaces rectangle-only final presentation while retaining a low-cost fallback renderer for tests.
- Windows keyboard and controller checks cover play, restart, daily mode, suspend/resume, and return.

## Shared definition of done

All three packages use the same engine lifecycle, input frame, renderer, asset lookup, text system, difficulty record, profile checkpoint, and event vocabulary. Game-specific generation, solving, rules, and metrics remain outside the runtime host. No package may carry a copied private font renderer, storage codec, or host-integration wrapper.
