# Blocks & Buttons

Blocks & Buttons is a calm workshop puzzle for short family play sessions. Move the robot through an 8x8 room and push every star block onto a red button.

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

Screen-aligned high-angle toy workshop. The grid is not rotated. Blue robot, silver arms, red beacon, golden star blocks, convex red buttons, and slate machinery walls all touch the bottom-center of their occupied cell. Native uses high-resolution pixel-art masters; PICO-8 uses independent 16x20 raised sprites.

Palette: [palette.json](palette.json).

## Acceptance

- Every room is unique, solvable, and has recorded minimum pushes.
- Undo, blocked pushes, deadlock recovery, persistence, advance, final completion, and return home work in both editions.
- Native uses a 480x480 arena plus workshop scenery; PICO-8 fills its 128x128 screen.
