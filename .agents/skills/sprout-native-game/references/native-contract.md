# Native package contract

Expected package surface:

```text
games/<slug>/
  game.lua
  manifest.json
  asset-manifest.json
  assets-src/
  assets/
```

Required qualities:

- package identity and API version are stable;
- files and assets are package-local and declared;
- simulation is deterministic at a fixed tick;
- input is action-based;
- render commands are bounded and batchable;
- state restoration is all-or-nothing;
- profile data cannot cross package or profile boundaries;
- missing or corrupt optional data degrades safely;
- title, gameplay, terminal, and return states are externally observable.

Useful canonical documents:

- `docs/runtime/README.md`
- `docs/runtime/engine-systems.md`
- `docs/runtime/game-lifecycle.md`
- `docs/specs/runtime-package-v1.md`
- `docs/specs/runtime-assets-v1.md`
- `docs/arcade/assets.md`
