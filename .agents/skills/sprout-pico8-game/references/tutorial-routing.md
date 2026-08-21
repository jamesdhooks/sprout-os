# PICO-8 tutorial routing

Use this index to find a focused worked example. Do not load the entire tutorial
library for every task, and do not copy tutorial code into a release cart without
adapting and testing it.

The official PICO-8 manual and Sprout cart contract remain authoritative. The
NerdyTeachers material is a practical learning aid whose examples may use a
different update rate, architecture, or compatibility target.

## Route by problem

| Need | Start here | Sprout follow-up |
| --- | --- | --- |
| Tile-world collision | [Map and Flags](https://nerdyteachers.com/PICO-8/Collision/98) | Define semantic flag constants, sample every relevant actor edge, and test at `_update60()` speed. |
| Screen and room bounds | [Boundary](https://nerdyteachers.com/PICO-8/Collision/97) | Apply bounds to the actor's collision box, not merely its sprite origin. |
| Non-tile collision | [Tutorial index](https://nerdyteachers.com/PICO-8/Tutorials/) collision section | Pick the cheapest primitive that matches play: point, rectangle, circle, or line. |
| Sprite animation | [Animate Sprite](https://nerdyteachers.com/PICO-8/Game_Mechanics/4) | Use named frame lists and integer frame indices; keep timing and state changes out of `_draw()`. |
| Momentum or platform motion | [Advanced Movement](https://nerdyteachers.com/PICO-8/game_mechanics/10) | Separate input intent, velocity, collision resolution, and animation state. |
| Movement model selection | [Player Movement Types](https://nerdyteachers.com/PICO-8/Game_Design/111) | Choose grid, direct, inertial, or path movement before tuning constants. |
| Screen impact | [Screen Shake](https://nerdyteachers.com/PICO-8/game_mechanics/11) | Shake the world camera briefly, reset camera state before UI, and offer a reduced-motion path where appropriate. |
| Particles and trails | [Particle Effects](https://nerdyteachers.com/PICO-8/Game_Mechanics/14) | Use a fixed pool or hard cap; visual randomness must not alter gameplay RNG. |
| Teaching controls and rules | [Onboarding Principles](https://nerdyteachers.com/PICO-8/Game_Design/105) and [Methods](https://nerdyteachers.com/PICO-8/Game_Design/106) | Prefer playable first levels, icons, short prompts, and help that fades after mastery. |
| PICO-8 audio basics | [Tutorial index](https://nerdyteachers.com/PICO-8/Tutorials/) audio section | Reserve channels by role and verify music/SFX behavior in fake-08. |

## Adoption rule

Before adapting an example:

1. Identify the single mechanic being learned.
2. Reproduce it in a scratch cart if its behavior is uncertain.
3. Translate it to the game's existing state and naming model.
4. Confirm `_update()` versus `_update60()` timing assumptions.
5. Replace magic sprite, flag, SFX, and color numbers with named constants.
6. Add a deterministic test or capture state for the resulting behavior.
7. Validate token, compressed-cart, and fake-08 compatibility budgets.

Do not add a generalized subsystem solely because a tutorial demonstrates one.
