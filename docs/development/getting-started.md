# Getting Started

Sprout currently has documentation only. There is no dependency manifest, build system, executable, test command, supported release, or deployable SD-card image.

## Tools

Current documentation work requires Git and a UTF-8 Markdown editor. Future launcher work is expected to require a Windows or Linux development host, an Onion-compatible cross-compilation toolchain, SDL development libraries, and secure file transfer to the device. Exact versions and commands must come from the pinned Onion investigation and an accepted implementation issue.

## Development cards

Maintain two physically separate SD cards:

- **Stable card:** known-working Onion/Sprout installation, real library, and real saves. Do not use it for experiments.
- **Development card:** pinned Onion version, representative legally supplied test ROMs, debug builds, logs, and enabled development access.

Never hot-swap a card while the device is powered or suspended. Back up the development card before changing startup, power, launch, or recovery behavior.

## Intended development loop

1. Reproduce behavior in a 640×480 desktop preview where possible.
2. Run deterministic unit and integration checks on the host.
3. Cross-compile with the verified Onion-compatible toolchain.
4. Deploy only the changed component to the development card.
5. Exercise launch, suspend, GameSwitcher, return, and recovery on hardware.
6. Capture exact commands, revisions, logs, and outcomes.

This is an intended workflow, not a current command reference.

## Configuration and secrets

Local configuration, device identifiers, credentials, profile images, saves, ROMs, BIOS files, logs containing private data, and exported household archives must remain outside Git. Commit sanitized examples only when a schema has an implementation consumer.

Application files and user data must remain separable so an update cannot overwrite profiles or saves. Configuration activation will require validation, atomic writes, snapshots, and last-known-good recovery.

## Preview, recovery, and limitations

Desktop preview should emulate the 640×480 display, action-level inputs, slow storage, offline operation, and representative data. It cannot verify Miyoo input devices, power behavior, framebuffer details, Onion process state, or GameSwitcher integration.

The development build must eventually support a documented boot gesture that bypasses Sprout and starts stock Onion, plus recovery after repeated launcher failures. The blueprint's suggested button is not accepted until hardware testing confirms the startup path.

See [Onion integration research](../research/onion-integration.md) for current evidence and unknowns.
