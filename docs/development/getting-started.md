# Getting Started

Sprout currently has a Windows desktop launcher preview, resumable offline first-run setup, and a versioned local profile repository. There is no supported release, Onion adapter, policy enforcement, or deployable SD-card image.

## Tools

Current launcher development requires:

- Git;
- PowerShell 5.1 or later;
- Visual Studio or Build Tools with the Desktop development with C++ workload, including CMake and Ninja; and
- network access during the first configure so CMake can fetch the pinned SDL2 and SQLite sources.

The launcher uses C++20, SDL2 `release-2.32.10` pinned to commit `5d249570393f7a37e037abf22cd6012a4cc56a71`, SDL2_image 2.8.8 pinned to commit `c1bf2245b0ba63a25afe2f8574d305feca25af77`, the SQLite 3.53.4 amalgamation pinned by its published SHA3-256, and yyjson 0.12.0 pinned to commit `7871d321ff4cd8068c1f777c97975dc2fb640ab3`. Build output and fetched dependencies remain under the ignored `out/` directory.

An Onion-compatible cross-compilation environment and device SDL backend are still unverified. They must be selected through the pinned Onion investigation rather than inferred from the desktop build.

## Windows desktop preview

Configure, build, and run both test targets:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\dev.ps1 -Action test
```

Run the interactive 640×480 preview:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\dev.ps1 -Action run
```

Use arrow keys or WASD to move, Enter/Space/Z to select, and Escape/Backspace/X to save setup progress and exit or to go back from the launcher. SDL-compatible controllers use the D-pad, A, and B. The development command stores sanitized preview configuration and profiles under the ignored `out/preview-data/` directory, so closing and reopening demonstrates resume. It does not read ROMs, device data, credentials, or live household data.

## Development cards

Maintain two physically separate SD cards:

- **Stable card:** known-working Onion/Sprout installation, real library, and real saves. Do not use it for experiments.
- **Development card:** pinned Onion version, representative legally supplied test ROMs, debug builds, logs, and enabled development access.

Never hot-swap a card while the device is powered or suspended. Back up the development card before changing startup, power, launch, or recovery behavior.

## Intended development loop

1. Reproduce behavior in the 640×480 desktop preview where possible.
2. Run deterministic unit and integration checks on the host.
3. Cross-compile with the verified Onion-compatible toolchain.
4. Deploy only the changed component to the development card.
5. Exercise launch, suspend, GameSwitcher, return, and recovery on hardware.
6. Capture exact commands, revisions, logs, and outcomes.

Only the Windows desktop configure/build/test/run commands above are currently implemented. The test action covers launcher navigation, profile persistence and migrations, atomic configuration recovery, interruption at every setup step, setup presentation behavior, and a headless SDL render traversal into the persisted profile selector. Cross-compilation and deployment steps remain intended workflow.

## Configuration and secrets

Local configuration, device identifiers, credentials, profile images, saves, ROMs, BIOS files, logs containing private data, and exported household archives must remain outside Git. Commit sanitized examples only when a schema has an implementation consumer.

Application files and user data must remain separable so an update cannot overwrite profiles or saves. Configuration activation will require validation, atomic writes, snapshots, and last-known-good recovery.

## Preview, recovery, and limitations

Desktop preview should emulate the 640×480 display, action-level inputs, slow storage, offline operation, and representative data. It cannot verify Miyoo input devices, power behavior, framebuffer details, Onion process state, or GameSwitcher integration.

The development build must eventually support a documented boot gesture that bypasses Sprout and starts stock Onion, plus recovery after repeated launcher failures. The blueprint's suggested button is not accepted until hardware testing confirms the startup path.

See [Onion integration research](../research/onion-integration.md) for current evidence and unknowns.
