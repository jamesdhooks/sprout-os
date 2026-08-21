# Starlight Snake

Starlight Snake is a quick garden chase with two modes. Guide one connected snake, collect fruit, and avoid the border and your own body.

## Shared rules

- D-pad queues one legal cardinal turn.
- Round mode wins at 12 fruit and returns home after its celebration.
- Endless mode continues until collision and records a separate best score.
- Left/right selects the mode on the title; A starts; B returns home.
- Holding B on the title resets best scores and completed-round counts with cancellable feedback.

The snake renderer chooses one of four heads, two straights, four corners, and four tapered tails from exact neighbour connectors. Active runs are session scoped; selected mode, mode-specific best scores, and completed rounds are profile scoped.

## Visual language

A moonlit storybook garden with violet sky, warm fruit, rose accents, and a teal connected snake. Decorative flowers and leaves are deterministic and never obscure the snake or fruit. Native gameplay uses a full 20x15 grid at 32 pixels per cell. PICO-8 uses a 14x14 garden inside a one-cell boundary.

Palette: [palette.json](palette.json).

## Acceptance

- Queued turns, fruit placement, speed changes, Round win, Endless collision, retry, persistence, reset, and return home work in both editions.
- Every connector joins at the same span after deterministic rotation.
