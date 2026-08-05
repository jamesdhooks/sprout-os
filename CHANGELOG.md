# Changelog

All notable user-visible changes to Sprout will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and released versions will follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html) once versioned software exists.

## [Unreleased]

### Changed

- Mouse & Cheese Maze now fills the gameplay viewport with deterministic
  seeded mazes, starts with simple low-density boards and large resampled art,
  scales through progressively denser board bands, retains one cell of calm
  forest-colored padding around every maze, anchors the mouse by its body
  rather than its tail-inclusive image bounds, uses smoothly interpolated held-
  direction movement, scales actors from their visible artwork bounds, renders
  walls above the mouse's overlapping tail, sizes its centered level tab to the
  active grid cell and aligns it flush with the top edge, smoothly resamples its
  high-resolution atlas, persists the current level, and advances automatically
  after a short, uncluttered "Cheese!" celebration.
- Mouse & Cheese Maze now varies its deterministic starting room and initial
  facing by level, then places the cheese at the farthest reachable cell from
  that start.
- Mouse & Cheese Maze maps each padded grid across the complete viewport with
  independent cell axes, keeping exactly one padding cell on every edge without
  stretching actor sprites.
- Mouse & Cheese Maze includes a concealed, time-limited breadcrumb hint that
  solves from the current cell and reveals at most half the route, capped at 12
  cells, using animated dots that fade with distance. Windows `H` synthesizes
  the portable hint chord while B/secondary remains a host-owned return action;
  hint buttons consume regular directional input for their complete held
  duration.
- Mouse & Cheese Maze continues increasing structural density throughout its
  campaign, growing from a 23x17 maze around level 100 to 53x39 near level 1,000
  instead of reusing the former 19x15 cap.
- Mouse & Cheese Maze advances one second after showing `Cheese!`, with its
  button-to-continue shortcut becoming available after 0.2 seconds.
- Mouse & Cheese Maze uses a larger content-width top-left numeric indicator
  built from the reusable rounded-panel primitive, with a soft shadow, golden
  rim, cream center, and layered Nunito ExtraBold numerals.
- Larger Mouse & Cheese Maze levels now use deterministic connected footprints
  with progressively more missing forest chunks and irregular edges. Dense
  layouts reserve the top-left indicator area as non-playable terrain while
  retaining the exact one-cell screen perimeter and full maze solvability.
  Hedge junctions render as a continuous green boundary; omitted cells render
  no tiles and expose the quiet forest background instead of brown terrain.
  The UI reservation activates when cells become smaller than the indicator
  and includes a one-cell right/bottom margin that scales with the active grid.
- Mouse & Cheese Maze now uses its warm dirt artwork for carved paths, giving
  the white mouse stronger contrast than the previous pale sand floor.
- The Windows launcher supports launcher-owned Arcade auto-launch for focused
  game iteration, preserving a real Sprout return destination behind the game.
- Native-game title screens can show package-owned progress and host a
  three-second hold-to-reset action. Mouse & Cheese Maze uses it to show the
  saved level and reset persisted progress to level 1 with a circular indicator.
- The native-game renderer supports reusable alpha-blended circle primitives
  for lightweight effects and indicators.
- The native-game renderer supports reusable filled rounded panels and the
  shared Nunito ExtraBold display face, avoiding package-local panel geometry
  and headline fonts.
- Mouse & Cheese Maze provides debounced action-level QA chords for jumping by
  1, 10, or 100 levels while inspecting its difficulty bands.
- Shared runtime labels now fit and clip text within both dimensions of their
  declared region; the Mouse Maze level tab expands for multi-digit levels.

### Added

- A cohesive storybook launcher and Arcade presentation with rounded Nunito
  typography, illustrated startup art, four parent-assignable profile scenes,
  simplified animated profile selection, high-resolution baked game-title art,
  and deterministic Windows visual review coverage.
- A shared runtime label primitive and high-resolution tile resampling so game
  HUDs, terminal states, and rich source atlases do not require package-local
  pixel fonts or low-resolution tile sources.
- A 64-item built-in profile-avatar library with 512-pixel transparent masters,
  runtime thumbnails, paged parent-managed selection, and retained custom-image
  import support.
- Playable Mouse & Cheese Maze and Blocks & Buttons Windows prototypes with
  deterministic storybook sprite atlases and reproducible asset packing.
- Strict package-local atlas, sprite-frame, animation, and tile-set manifests;
  fixed-tick animation; compact tilemap submission; general sprite/animation
  batch submission; lazy PNG texture caching; target-aware resampling; and
  same-atlas SDL geometry batching.
- A CI-exercised Windows Arcade lifecycle journey covering explicit native-package discovery, real runtime process start, structured play events, profile/package-isolated storage, normal exit, and launcher return, with a manual keyboard/controller/render evidence protocol.
- Local Sprout Arcade discovery in the family launcher, manifest-based child audience controls, typed native launch handoff, profile-isolated game storage, clean launcher return, and a deterministic launcher-to-runtime Windows smoke check.
- A committed Windows-only Sprout Arcade preview milestone that keeps physical Onion acceptance open while bounding native runtime work to one local packaged game and its verified launcher lifecycle.
- A Windows-native Sprout Runtime preview with strict local manifests, sandboxed text Lua, deterministic fixed-step sessions, action input, rectangle rendering, structured events, atomic package storage, and lifecycle instruction limits.
- Snake as the first local Arcade package, with deterministic D-pad play, immediate replay, best-score storage, storybook presentation, and automated runtime coverage.
- A pinned Onion artifact audit that records SHA-256, byte size, ARM EABI5 format, interpreter, and reviewed dynamic dependencies and fails before deployment on incompatible output.
- A deterministic host integration journey and release-blocking evidence checklist spanning resumed setup, profiles, parent access, local GB/SNES selections, time expiry, profile portability, and repeated-start recovery while preserving hardware-only gates.
- A controller-accessible desktop recovery flow after repeated unfinished starts, with validated last-known-good preview/restore, cancel-first launcher reset, collision-safe active-configuration quarantine, and readiness only after ordinary UI renders.
- A clock-independent launcher startup-health store that transactionally counts unfinished attempts, rejects stale readiness acknowledgements, and requests recovery after three consecutive failures.
- A controller-accessible, parent-reauthenticated one-profile export and restore flow with versioned checksummed archives, conflict preflight, explicit unencrypted-portrait consent, and no secrets, usage, saves, or library activity.
- A profile-scoped daily-time-policy core with monotonic active accounting, persisted usage and one-time warnings, bounded restart recovery, rollback checks, and explicit launch-block/save-and-exit decisions.
- A reproducible Onion ARM cross-build, pinned compiler/sysroot and CMake inputs, CI coverage, isolated device diagnostic, and development-card validation protocol.
- Deterministic local Onion GB/SNES discovery plus controller-driven recent, favorite, and all-game views with fail-closed unavailable states.
- A typed Onion GB/SNES launch adapter with canonical path, policy, extension, launcher, and structured process-outcome validation, plus desktop contract tests.
- Controller-based parent PIN setup and entry, Argon2id storage, authenticated reboot-persistent end-of-day grants, sensitive-action reauthentication, manual lock, and clock-rollback checks.
- Local parent-profile image import with controller-driven cropping, versioned managed PNG variants, metadata removal, safe replacement, and persisted portrait rendering in the desktop preview.
- Resumable offline first-run setup with atomic versioned configuration, last-known-good recovery, built-in parent/child profiles, and a 640×480 desktop presentation.
- Versioned SQLite profile persistence with parent/child validation, reversible archive and restore, last-active-parent protection, and migration rollback tests.
- Initial 640×480 desktop launcher preview with deterministic parent/child fixtures, action-level keyboard and controller input, and navigation/render smoke tests.
