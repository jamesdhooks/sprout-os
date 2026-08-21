# PICO-8 cart contract

Expected source:

```text
pico8/<slug>/
  <slug>.p8
  manifest.json
  README.md
  library-cover.png
  optional tracked generator specifications and approved campaign data
```

Key constraints:

- fixed 128x128 screen;
- 16-colour PICO palette;
- standalone text cart;
- licensed PICO-8 compiler is authoritative for token and compressed-cart limits;
- public development save ID is distinct from Sprout profile-specific materialized IDs;
- desktop success is not fake-08 or Miyoo evidence;
- title image, launcher cover, and gameplay sprites are separate compositions;
- QA capture modifies only a temporary cart.

Desktop defaults map A to Z and B to X. Release gameplay must remain controller-complete.
