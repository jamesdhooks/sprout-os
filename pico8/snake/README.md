# Starlight Snake PICO-8 Edition

Standalone PICO-8 port of Sprout Snake. D-pad steers, Z/A starts or retries,
and X/B returns to the title. Collecting 12 fruit completes a round and shows
an animated celebration card before returning to the beginning. Collision
retains a separate retry card.

The snake is rendered as one connected form: outlined straight and corner
joins, an oriented head, and a tapered tail. `sprites.json` is the canonical
hand-authored 8x8 source for every segment, fruit, and garden detail. The title
uses the same pieces at 2x scale, so its visual promise matches gameplay.

After editing a pixel grid, run:

```powershell
python tools/build_snake_character_assets.py
python tools/build_snake_character_assets.py --check
```

Run `python tools/pico8_arcade_build.py inject pico8/snake/snake.p8` after
changing the shared arcade core.
