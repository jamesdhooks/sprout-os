# Blocks & Buttons PICO-8 Edition

Standalone PICO-8 port of the Sprout Arcade crate puzzle. D-pad moves, Z/A
starts or retries, and X/B returns to the title. Seating both crates triggers
an animated celebration card, then returns to the beginning.

The live renderer is a depth-sorted pseudo-isometric toy workshop. Every floor
cell, wall, button, crate, and robot uses the same high-angle projection, with
vertical faces for readable height and overlap.

Run `python tools/pico8_arcade_build.py inject pico8/blocks-buttons/blocks-buttons.p8`
after changing the shared arcade core.
