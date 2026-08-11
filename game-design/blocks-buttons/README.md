# Blocks & Buttons

Blocks & Buttons is a calm workshop puzzle for short family play sessions. Move the robot through an 8x8 room and push every crate onto a red button.

## Shared rules

- D-pad moves one cell; a block moves only when the cell beyond it is open.
- A restores the immediately previous valid move, including a push.
- A retries after a confirmed deadlock; B returns home.
- A room is complete when every block occupies a button.
- The 30-room campaign advances automatically after a short workshop celebration.
- Room progress, positions, moves, pushes, and completion are profile scoped.

The tracked campaign is generated offline from solved states and proven with a forward solver. Runtime code reads accepted resolved layouts and precomputed dead squares; it never searches for campaign candidates.

## Campaign bands

| Rooms | Blocks | Intent |
| --- | --- | --- |
| 1-8 | 1 | Learn alignment, backing up, and safe pushes. |
| 9-20 | 2 | Reposition and choose a block order. |
| 21-30 | 2-3 | Plan deeper push sequences around tempting dead squares. |

## Visual language

Screen-aligned, high-angle top-down toy workshop. The grid is never rotated, but the robot may rise above its collision footprint while remaining bottom-centre anchored. Its south view shows a pale face panel with two eyes, its north view shows the rear shell, and its side view is a true screen-cardinal profile. The identity is a compact blue body, articulated silver arms and clamps, tiny feet, and a red/cyan crown beacon. Crates are square wooden X-braced tops, buttons are one red cap inside exactly one silver ring, and walls are one solid blue-steel surface with corner bolts rather than nested panels. Native uses high-resolution pixel-art masters; PICO-8 uses an independent, hand-refined 16x16 interpretation of the same character.

Palette: [palette.json](palette.json).

## Acceptance

- Every room is unique, solvable, and has recorded minimum pushes.
- Undo, blocked pushes, deadlock recovery, persistence, advance, final completion, and return home work in both editions.
- Native uses a 480x480 arena plus workshop scenery; PICO-8 fills its 128x128 screen.
