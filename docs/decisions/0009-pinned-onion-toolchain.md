# ADR 0009: Pin the Onion cross-compilation toolchain

- Status: Accepted
- Date: 2026-08-02

## Context

Sprout needs a repeatable ARM build before launcher code can be validated on a Miyoo Mini Plus. Onion `v4.3.1-1` builds native components with `aemiii91/miyoomini-toolchain:latest`, but a mutable tag cannot identify the compiler and sysroot used for a Sprout artifact. The image's bundled CMake 3.13.4 is also older than Sprout's build definition.

## Decision

Use the Linux AMD64 manifest `sha256:a8da1021449c80c0ccb75e263f1dfc75b5a004278fefa8a54151e55698a352f4` from Onion's toolchain image and run it through Docker. Use its GCC 8.3.0 `arm-linux-gnueabihf` compiler, glibc 2.28 sysroot, and Onion's ARM flags:

```text
-marm -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -march=armv7ve
```

Supply CMake 3.31.12 from Kitware's Linux x86-64 archive, verified by SHA-256 `0dc2e9a6860f06bf10bd8fadc03e35d9eeb4df46e33763a7e480e987758f385c`. Keep the C++20 language mode, but provide small project-owned compatibility helpers for standard-library conveniences absent from GCC 8. Link `libstdc++` and `libgcc` statically; retain the target's glibc, math, and pthread runtime dependencies.

The first target is `sprout-onion-check`, an isolated command-line diagnostic for storage, credentials, launch-request construction, and optional read-only library discovery. It is not the device launcher or a renderer.

## Alternatives considered

- **Use the mutable `latest` tag:** matches Onion's Makefile text, but cannot reproduce an artifact after the tag changes.
- **Install a newer independent ARM toolchain:** improves language-library support, but risks producing binaries incompatible with Onion's target sysroot.
- **Lower the entire project language level:** removes useful compile-time constraints from supported hosts when only a few library conveniences need compatibility helpers.
- **Build a device renderer now:** would combine compiler validation with unverified SDL, input, process, and recovery behavior.

## Consequences

- Windows and Linux hosts with Docker and PowerShell can produce the same pinned ARM diagnostic.
- The build downloads a pinned CMake archive and source dependencies into ignored `out/` paths.
- CI verifies that the portable launcher boundary continues to compile and link for Onion's ARM target.
- Image contents cannot be reconstructed from the Docker manifest alone if the registry removes them; a release process must archive or replace the toolchain before depending on long-term availability.
- Device execution, performance, SDL rendering, input, GameSwitcher behavior, and safe recovery remain hardware-validation work.
