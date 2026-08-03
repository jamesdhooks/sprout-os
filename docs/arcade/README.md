# Sprout Arcade

Status: **First collection prototypes and Windows engine development**

Sprout Arcade is the native-game collection for Sprout. The first collection is a deliberately small set of readable D-pad-and-button games that work offline, use deterministic content, respect family profiles, and share a portable runtime instead of shipping bespoke executables.

The current implementation includes three discoverable local packages. Sprout
Snake proves the original lifecycle kernel; Mouse Maze and Blocks & Buttons add
playable first rooms, reproducible pixel-art atlases, fixed-tick animation,
compact tilemaps, and general same-atlas sprite batching. These remain
prototypes rather than their required 1,000-level campaigns. Windows is the
only verified game target. Linux, browser, Raspberry Pi, Miyoo, Onion, suspend
behavior, and device performance remain future validation tracks.

## Collection principles

- A child should understand the primary action within one screen.
- A round should start quickly and restart without menu traversal.
- D-pad plus primary/secondary actions is the baseline.
- Difficulty comes from explicit parameters and measured content metrics.
- Seeds reproduce procedural content and debugging sessions.
- Campaign layouts are generated and validated offline when runtime generation would be expensive or risky.
- Profiles own progress, difficulty state, achievements, recents, favorites, and saves.
- Game code uses shared engine systems; it does not access SDL, the filesystem, clocks, or platform services directly.
- New engine systems are added for concrete games, not for the full speculative catalogue.

## Documentation map

- [Catalogue](catalogue.md): proposed 15-game set, implementation order, and shared-system coverage.
- [First three games](first-three-games.md): complete v1 requirements and acceptance criteria for Mouse & Cheese Maze, Blocks & Buttons, and Snake.
- [Content pipeline](content-pipeline.md): deterministic generation, solving, scoring, curation, and campaign export.
- [Asset pipeline](assets.md): pixel-art constraints, provenance, generation, packing, and validation.
- [Runtime overview](../runtime/README.md): reusable engine boundary and implementation sequence.
- [Windows evidence](../development/arcade-windows-evidence.md): automated lifecycle matrix and manual input/render checklist.
- [Roadmap](../roadmap.md): public milestone status.

Remote catalogue browsing, downloads, purchases, package signing, publishing, and arbitrary third-party packages are outside the first collection. Only reviewed local packages are supported until those trust boundaries are designed and accepted.
