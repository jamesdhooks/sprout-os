# Starlight Snake PICO-8 Edition

Standalone PICO-8 port of Sprout Snake. D-pad steers, Z/A starts or retries,
and X/B returns to the title. Collecting 12 fruit completes a round and shows
an animated celebration card before returning to the beginning. Collision
retains a separate retry card.

The snake is rendered as one connected form: outlined straight and corner
joins, an oriented head, and a tapered tail. `sprites.json` is the canonical
hand-authored 8x8 source for every segment, fruit, and garden detail. A 14x14
playable garden plus one 8-pixel boundary cell fills the complete 128x128
display without a score strip or fractional sprite scaling.

The title is deliberately cover art rather than a reconstruction of gameplay.
`title-source.png` retains the full-resolution illustration and
`title-screen-pico.png` is the reviewed 128x128 PICO-palette conversion. Its
compressed screen payload does not consume the gameplay sprite bank.

After editing a pixel grid, run:

```powershell
python tools/build_snake_character_assets.py
python tools/pico8_title_assets.py --source pico8/snake/title-source.png --cart pico8/snake/snake.p8 --preview pico8/snake/title-screen-pico.png
python tools/pico8_title_assets.py --check --source pico8/snake/title-source.png --cart pico8/snake/snake.p8 --preview pico8/snake/title-screen-pico.png
python tools/build_snake_character_assets.py --check
```

Run `python tools/pico8_arcade_build.py inject pico8/snake/snake.p8` after
changing the shared arcade core.
