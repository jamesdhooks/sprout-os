# ADR 0007: Use portable C++ with an SDL2 desktop adapter

- Status: Accepted
- Date: 2026-08-02

## Context

The launcher needs a real 640×480 desktop target with deterministic navigation tests while the Miyoo rendering and cross-compilation path remains unverified. Onion's own applications demonstrate C and SDL 1.2 on-device, and maintained community launchers demonstrate that SDL2 device paths exist, but neither fact proves the correct Sprout hardware backend.

The first implementation must avoid binding profile, navigation, and lifecycle behavior to a desktop windowing API or an imagined device adapter.

## Decision

Implement launcher domain state in portable C++20 without SDL dependencies. Use SDL2 only in the desktop executable for windowing, rendering, keyboard input, and controller input. Pin SDL2 `release-2.32.10` to upstream commit `5d249570393f7a37e037abf22cd6012a4cc56a71` through CMake FetchContent.

The device renderer remains a separate evidence-driven decision. It may use a compatible SDL2 port, SDL 1.2, or a narrower platform backend without changing launcher navigation state.

## Alternatives considered

- **SDL 1.2 everywhere:** closest to Onion's current native applications, but weakens the desktop development path and still does not prove the custom launcher device integration.
- **Browser/TypeScript preview:** immediately convenient on the host, but creates a likely throwaway implementation separate from the native launcher.
- **Adopt an existing launcher wholesale:** proves feasibility but imports unrelated lifecycle, settings, and distribution decisions before Sprout has a narrow integration contract.

## Consequences

- The desktop preview and state tests build on Windows without Onion or a server.
- SDL2 uses the zlib license, is actively maintained upstream, and is built statically for the preview. The observed unoptimized Windows debug executable is approximately 4.1 MiB; release/device size remains unmeasured.
- First configure requires network access to fetch the pinned source; subsequent builds use the local CMake dependency checkout.
- A successful desktop build is not evidence of Miyoo compatibility. Cross-compilation, input, framebuffer, power, and return behavior remain hardware-validation work.
