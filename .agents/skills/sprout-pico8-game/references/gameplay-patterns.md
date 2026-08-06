# Sprout PICO-8 gameplay patterns

These patterns adapt common PICO-8 teaching examples to Sprout's small-screen,
controller-first, child-friendly games. They are constraints, not a framework.

## Simulation order

Use a predictable `_update60()` sequence:

1. Read input into intent or requested actions.
2. Advance state timers.
3. Update velocity or the active grid transition.
4. Resolve world collision one axis or one grid edge at a time.
5. Resolve goals, hazards, and state transitions.
6. Advance animation from the resolved state.
7. Update bounded cosmetic effects from their own random stream.

Render the resulting state in `_draw()`. Drawing must not decide movement,
collision, progress, or persistence.

## Movement

Choose one movement contract explicitly:

- **Grid-step:** accept one destination at a time, interpolate position at a
  constant or eased visual rate, and buffer at most one next direction.
- **Direct:** held input produces immediate velocity; diagonal speed is clamped
  when diagonal motion exists.
- **Inertial:** input changes velocity, friction/gravity changes it further, and
  a maximum bounds it.
- **Path-driven:** simulation owns a route and input chooses or interrupts it.

Do not rely on host keyboard repeat. Use `btn()` for continuous intent and
`btnp()` for one-shot actions. A grid actor must not repeatedly accelerate and
stop at cell boundaries unless that pulse is an intentional design choice.

## Collision

- Use map flags when interaction belongs to a tile category rather than a
  particular sprite number.
- Keep world positions in pixels and convert to tile coordinates only at the
  collision boundary.
- Give actors an explicit collision box independent of decorative sprite
  overscan.
- Sample the leading edge after proposed motion, then resolve before committing
  position. Split X and Y resolution for freely moving actors.
- Use point/circle/rectangle tests for dynamic objects instead of forcing them
  into map flags.
- Keep visual variants collision-equivalent unless the level data explicitly
  says otherwise.

## Animation and anchors

- Store `facing`, `motion_state`, `frame_index`, and `frame_timer` explicitly.
- Define named frame lists instead of assuming adjacent sprite numbers.
- Reset or retain the phase deliberately on state transitions; never let a
  fractional sprite number select an unrelated atlas entry.
- Advance locomotion frames only while resolved displacement is non-zero.
- Bottom-anchor raised or tilted sprites to their logical cell. Collision stays
  in the footprint even when art extends upward.
- Test every facing at cell edges, corners, and behind foreground layers.

## Feedback without clutter

- Treat particles, shake, flashes, and animated text as confirmation of an
  already-readable event.
- Cap particle count and lifetime. Prefer compact records and deletion-safe
  reverse iteration or a fixed pool.
- Keep cosmetic randomness separate from procedural level or gameplay RNG.
- Apply camera shake to the world, restore `camera()`, then draw stable UI.
- Provide high-contrast readable states before adding motion or decoration.

## Onboarding for Sprout players

- Put the player into safe interaction quickly.
- Introduce one action or rule in isolation, repeat it with a variation, then
  test it in a real challenge.
- Use level geometry, animation, and button icons before paragraphs.
- Keep prompts short and remove them once the demonstrated action succeeds.
- Retain an accessible control reminder on title or pause screens.
- Offer hints after inactivity or request; do not punish a child for using one.
- Validate onboarding with someone who has not watched development.

## Audio

- Name SFX and music slots by role in code.
- Give move, success, failure, and UI confirmation distinct shapes and priority.
- Prevent low-value repeated sounds from constantly cutting off important cues.
- Keep short loops tolerable over repeated play and test through Miyoo speakers.
- Treat fake-08 playback as a release check, not an inference from desktop.
