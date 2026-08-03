# Getting Started

Sprout currently has a Windows desktop launcher preview, resumable offline first-run setup, versioned local profiles, device-local parent access, portable one-profile backup/restore, persisted daily child-time and startup-health decision cores, desktop configuration recovery, deterministic local GB/SNES discovery, and a desktop-tested Onion launch contract. There is no supported release, hardware-validated Onion execution, policy lifecycle, safe-boot integration, or deployable SD-card image.

## Tools

Desktop launcher development requires:

- Git;
- PowerShell 5.1 or later;
- Visual Studio or Build Tools with the Desktop development with C++ workload, including CMake and Ninja; and
- network access during the first configure so CMake can fetch the pinned SDL2 and SQLite sources.

The launcher uses C++20, SDL2 `release-2.32.10` pinned to commit `5d249570393f7a37e037abf22cd6012a4cc56a71`, SDL2_image 2.8.8 pinned to commit `c1bf2245b0ba63a25afe2f8574d305feca25af77`, the SQLite 3.53.4 amalgamation pinned by its published SHA3-256, yyjson 0.12.0 pinned to commit `7871d321ff4cd8068c1f777c97975dc2fb640ab3`, and the official Argon2 reference release `20190702` pinned to commit `62358ba2123abd17fccf2a108a301d4b52c01a7c`. Build output and fetched dependencies remain under the ignored `out/` directory.

The ARM diagnostic cross-build additionally requires Docker with Linux AMD64 container support. It pins the Onion-derived compiler image and a verified CMake archive; see [ADR 0009](../decisions/0009-pinned-onion-toolchain.md). The device SDL backend remains unselected.

## Windows desktop preview

Configure, build, and run both test targets:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\dev.ps1 -Action test
```

Run the interactive 640×480 preview:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\dev.ps1 -Action run
```

By default, recent, favorite, and all-game views use sanitized in-memory fixtures. To inspect filenames on an explicitly selected development-card or fixture root without executing games:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\dev.ps1 -Action run -SdRoot F:\path\to\development-card
```

The opt-in scan reads directory entries beneath `Roms/GB` and `Roms/SFC`; it does not open ROM contents or inspect archives. Discovered child items are unavailable until profile allowlists are implemented. Parent selection emits a typed preview request to the console, and the Windows preview never executes Onion shell scripts.

Use arrow keys or WASD to move, Enter/Space/Z to select, and Escape/Backspace/X to save setup progress and exit or to go back from the launcher. SDL-compatible controllers use the D-pad, A, and B. The same directional controls operate the PIN keypad; PIN digits are masked. During portrait cropping, Q/E or the controller shoulder buttons zoom out/in.

The development command stores sanitized preview configuration, profiles, and managed portraits under the ignored `out/preview-data/` directory, so closing and reopening demonstrates resume. To exercise image import during first-run setup, place one BMP, JPEG, or PNG at `out/preview-data/imports/profile-image.<extension>` before reaching the portrait step. This explicit staging folder is the only image source location the preview checks; source paths are not retained.

The reauthenticated parent menu exports a selected profile to `out/preview-data/exports/` and scans direct `.sprout-profile` files in `out/preview-data/imports/` for restore. Export never overwrites an existing file. Move a test archive between those folders deliberately when exercising restore; archives are unencrypted and must stay out of Git. Unless `-SdRoot` is explicitly supplied, the preview does not read ROMs, device data, credentials, or live household data.

## Pinned Onion ARM diagnostic

Configure and build the command-line device diagnostic:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\build-onion.ps1 -Action build
```

This produces `out/build/onion-arm/launcher/sprout-onion-check`. It verifies that portable launcher storage, parent access, daily-time policy, a built-in-avatar profile archive round trip, startup-health persistence, library presentation, and typed launch-request code compile and link against Onion's ARM sysroot. It does not provide a device renderer, install a startup launcher, select a safe-mode action, or execute a game. Follow the [development-card device check](onion-device-check.md) before running it on hardware.

## Development cards

Maintain two physically separate SD cards:

- **Stable card:** known-working Onion/Sprout installation, real library, and real saves. Do not use it for experiments.
- **Development card:** pinned Onion version, representative legally supplied test ROMs, debug builds, logs, and enabled development access.

Never hot-swap a card while the device is powered or suspended. Back up the development card before changing startup, power, launch, or recovery behavior.

## Intended development loop

1. Reproduce behavior in the 640×480 desktop preview where possible.
2. Run deterministic unit and integration checks on the host.
3. Cross-compile with the pinned Onion-compatible toolchain.
4. Deploy only the changed component to the development card.
5. Exercise launch, suspend, GameSwitcher, return, and recovery on hardware.
6. Capture exact commands, revisions, logs, and outcomes.

The Windows test action covers launcher navigation, profile persistence and migrations, atomic configuration recovery, repeated-start routing decisions, recovery restore/reset presentation and quarantine safety, interruption at every setup step, setup presentation behavior, image decoding/cropping/activation, Argon2id PIN storage, authenticated grants, controller PIN entry, parent authorization transitions, profile archive validation/export/restore and presentation, persisted daily-time accounting, deterministic local GB/SNES discovery, recent/favorite/all presentation, the typed Onion launch contract, and a headless SDL render traversal. The pinned ARM build is also implemented and checked in CI. Device deployment is manual and hardware acceptance remains outstanding.

## Configuration and secrets

Local configuration, device identifiers, credentials, profile images, saves, ROMs, BIOS files, logs containing private data, and exported household archives must remain outside Git. Commit sanitized examples only when a schema has an implementation consumer.

Application files and user data must remain separable so an update cannot overwrite profiles or saves. Configuration activation will require validation, atomic writes, snapshots, and last-known-good recovery.

## Preview, recovery, and limitations

Desktop preview should emulate the 640×480 display, action-level inputs, slow storage, offline operation, and representative data. It cannot verify Miyoo input devices, power behavior, framebuffer details, Onion process state, or GameSwitcher integration.

The desktop launcher now recovers after repeated unfinished starts through validated last-known-good restore or launcher-only reset. The development build must still support a documented boot gesture that bypasses Sprout and starts stock Onion. The blueprint's suggested button is not accepted until hardware testing confirms the startup path.

See [Onion integration research](../research/onion-integration.md) for current evidence and unknowns.
