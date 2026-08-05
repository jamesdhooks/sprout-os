# Mouse & Cheese for PICO-8

This is the PICO-8 edition of Mouse & Cheese. The `.p8` cart is the only
runtime dependency; it is deliberately standalone for Onion fake-08.

`campaign.json` is the reviewed offline source of the 100-level campaign.
`tools/pico8_mouse_cheese_campaign.py` validates it and injects its compact
seed payload into the marked region of `mouse-cheese.p8`.

The normal public cart uses a development storage identifier. Sprout deployment
creates a managed copy with a profile-specific `cartdata()` identifier, so
children do not share campaign progress.
