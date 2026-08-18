# Game discovery and review

## Purpose

The game guide is the parent-only layer for working through a large local
library. It answers four questions with controller-first, low-text surfaces:

- What should I try next?
- What have I played but not reviewed?
- Which games have I approved or finished?
- Which games can each child see?

The child experience remains launch-first. Children do not see review,
curation, hidden-library, or assignment controls.

## Navigation

`Family Dashboard` opens the guide behind the existing parent-access boundary.
The main surface stays flat and uses the same vertical Netflix-style rails as
the game library:

1. Next Up
2. Recent
3. Stats
4. Platforms
5. Review
6. Unopened
7. Finish Next
8. Recommended
9. All Games
10. Hidden, when populated

Up and down move between rails. Left and right move within a rail. A opens a
game or platform. X opens the global filter drawer, Y clears active filters,
and the shoulder buttons apply a quick thumbs-down or thumbs-up review.

The filter applies to every rail and to the stats denominator. Platform is a
multi-select filter; Review, Progress, Family, child membership, and Visibility
are single-select categories.

## Game page

Each inventory item has one detail surface with three shoulder-switched pages:

- Overview: artwork/screenshots, platform, verified play time, and Play.
- Review: thumbs up, thumbs down, and completed/won.
- Family: recommended, for kids, hidden, and one toggle per child.

Up and down cycle available artwork on Overview and move through toggles on the
other pages. The UI uses platform, verdict, and trophy glyphs in preference to
labels. Text remains for titles, accessibility, and ambiguous destructive
states.

`Recommended` means part of the household-curated set. `For Kids` is a broad
household classification. Child assignment is exact membership for a specific
profile. `Hidden` removes an item from ordinary parent and child library views;
it also clears Recommended and cannot coexist with it.

Reviews and completion are per parent profile. Household curation and child
assignment are shared household state.

## Statistics

Stats are computed from the same filtered inventory used by the rails:

- Curated review percentage = reviewed Recommended games / Recommended games.
- Library review percentage = reviewed visible games / visible games.
- Outside = reviewed games that are not Recommended.
- Won = games marked completed by the active parent.

Unavailable catalogue items remain in history but are excluded from the normal
current-library views. Hidden items use their own visibility filter and rail.

## Recommendations

Next Up is deterministic and explainable. It prioritizes:

1. Recommended but unreviewed.
2. Played but unreviewed.
3. Started but incomplete.
4. Never opened.

The queue interleaves platforms when possible so one large ROM collection does
not crowd out the rest of the library. Dedicated Review, Unopened, and Finish
Next rails keep each reason visible without explanatory paragraphs.

## Persistence and migration

`game-library.sqlite3` is the durable source for discovered inventory,
screenshots, household flags, per-profile state, child membership, and play
sessions. Inventory reconciliation is non-destructive: missing catalogue items
become unavailable instead of being deleted.

The legacy household seed imports once. Existing favourites become profile
favourites, the curated union becomes Recommended and For Kids, and legacy
child lists become exact child membership. The migration stores an idempotent
metadata flag and reports unresolved seed entries instead of guessing.

## Activity integrity

Only terminal sessions contribute to launch count, Recent, or play time. Native
games are recorded only after the runtime process actually starts and returns;
abnormal exits remain valid played sessions. Active sessions are excluded and
are closed as interrupted during startup recovery.

The current Onion handoff proves a validated launch request, not an
authoritative game-start or return interval. It therefore does not create
activity. Emulated play time must remain empty until the Onion/GameSwitcher
wrapper supplies verified lifecycle checkpoints; a successful handoff alone is
not evidence of play.

## Platform identity and icons

Platform IDs are stable data values independent of presentation. Sprout Arcade
and emulated Onion Arcade are separate platforms. The launcher bundles simple
code-native glyphs for handheld, console, arcade, PICO-8, and Sprout categories,
with a short platform label beside ambiguous glyphs. No external theme or
trademarked logo is required for a usable interface.
