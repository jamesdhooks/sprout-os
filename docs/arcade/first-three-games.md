# First Three Arcade Games

Mouse & Cheese Maze, Blocks & Buttons, and Starlight Snake are the committed
consumers of Sprout Runtime v1. They share runtime services and design language,
not game rules or copied implementation layers. Native and PICO-8 editions are
standalone implementations of the same game concepts.

## Mouse & Cheese Maze

The native edition preserves its deterministic full-screen hedge-and-dirt maze
renderer, irregular connected footprints, seeded start and goal variation,
animated half-route hint, progressive density, and per-profile progress. The
mouse is bottom-centred with directional idle/run animation and deliberate tail
overscan; the cheese uses a readable triangular profile and sparkle animation.
Reaching it presents a game-specific pop-and-bob `Cheese!` card before the next
maze loads automatically.

The native campaign retains its existing progression. The PICO-8 edition ships
a separate 100-level deterministic campaign. Its renderer uses two deliberate
sprite families: 12x12 for the large tier and 9x9 source art for later tiers.
PICO level seeds and metrics remain offline-validated; campaign search never
runs on the device.

Both editions provide:

- smooth held-direction movement without an initial platform repeat delay;
- A for a bounded BFS hint whose dots fade with distance;
- B/back for host return during play;
- a three-second, cancellable B hold on the title to reset progress;
- reset input release and fresh-press requirements;
- deterministic QA level jumps and capture states;
- profile-scoped current-level and completion persistence.

The shared game-design record is [Mouse & Cheese](../../game-design/mouse-cheese/README.md).

## Blocks & Buttons

Move the workshop robot through an 8x8 room and push every star block onto a
red button. A moves one step of history backward during active play. A blocked
crate on a precomputed dead square produces the calm `Crate stuck` retry state;
this is not limited to visually obvious corners.

Both editions consume the same tracked 30-room campaign. The offline workflow
starts from solved states, applies legal reverse pulls, proves each result with
a forward push solver, rejects duplicates and trivial layouts, and records
minimum pushes, direction changes, crate dependencies, dead squares, solution
signatures, and layout hashes. Runtime code consumes compact validated rooms
and never executes campaign search.

Progress persists the campaign schema, room, robot and crate positions, move
and push counts, and completion. Rooms 1-8 introduce one-crate positioning;
9-20 add two-crate ordering; 21-30 use two or three crates with deeper planning
and safe deadlock temptations. A workshop celebration advances automatically;
the final card records campaign completion and returns to the title flow.

Native presentation uses a 480x480, 60-pixel-cell arena with 80-pixel workshop
scenery columns, bottom-anchored raised props, and row sorting. The PICO cart
uses 16-pixel collision cells with 16x20 robot, wall, and crate art; east is a
runtime mirror of west.

The shared game-design record and generation thresholds are in
[Blocks & Buttons](../../game-design/blocks-buttons/README.md).

## Starlight Snake

Starlight Snake offers two modes in both editions:

- **Round:** collect 12 fruit for an explicit win;
- **Endless:** continue until a wall or body collision and retain a separate
  best score.

Left/right selects a mode on the title, A starts or continues, and a deliberate
three-second B hold resets best scores and completed-round counts. Active runs
are session-scoped. Persisted data contains the schema, selected mode,
mode-specific best scores, and completed rounds.

The snake uses four facing heads, straight bodies, deterministic rotated
corners, four tapered tails, and stable connector contracts. Queued legal turns,
free-cell fruit placement, speed stages, eat bursts, readable mode-specific
win/fail cards, and original cues are part of both editions. Native play fills
a 20x15 grid at 32 pixels per cell with backed overlay chips; PICO play uses a
14x14 garden inside a one-cell boundary.

Daily mode and active-run restoration are deferred. The shared design record is
[Starlight Snake](../../game-design/snake/README.md).

## Shared definition of done

Every edition must pass deterministic gameplay tests, package and palette
validation, title/reset-state checks, representative progression captures, and
desktop play review. Native packages additionally use shared atlas, mip,
sampling, SFX, input, persistence, and title-reset services. PICO carts remain
single-file, budget-valid, independently authored cartridges compatible with
the licensed desktop runtime and the selected Onion emulator path.
