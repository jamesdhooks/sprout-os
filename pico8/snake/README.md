# Starlight Snake PICO-8 Edition

Standalone PICO-8 port of Sprout Snake. D-pad steers, Z/A starts or retries,
and X/B returns to the title. Collecting 12 fruit completes a round and shows
an animated celebration card before returning to the beginning. Collision
retains a separate retry card.

The snake is rendered as one connected form: outlined straight and corner
joins, highlighted body nodes, an oriented head, and a tapered tail. The board
uses a quiet moon-garden field, vine boundary, and rotating fruit artwork.

Run `python tools/pico8_arcade_build.py inject pico8/snake/snake.p8` after
changing the shared arcade core.
