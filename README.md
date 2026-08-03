# Sprout

Sprout is an open-source, family-oriented gaming platform for retro handhelds and small computers. It aims to provide family profiles, parental controls, an Onion-compatible emulation foundation, and a portable native-game runtime without making basic use depend on a server.

> **Status: early development.** This repository contains the project foundation and a Windows desktop launcher preview with resumable offline setup, versioned local profiles, parent access, portable one-profile backup/restore, daily-time policy storage, repeated-start configuration recovery, local GB/SNES library views, and local Sprout Arcade discovery and runtime handoff. These host implementations are not a device build.

## Principles

- Family profiles with isolated preferences, history, and saves where the platform permits it.
- Parental controls that work locally and degrade safely.
- Onion-compatible emulation rather than a replacement emulator stack.
- A deterministic, portable runtime for small native games.
- Provider-neutral integrations based on explicit capabilities.
- Offline-first operation with optional synchronization and services.

## Component map

| Component | Responsibility | Status |
| --- | --- | --- |
| SproutOS | Family launcher, profiles, policy, library, Onion integration, and recovery | First committed milestone |
| Sprout Runtime | Portable native-game execution and reusable 2D engine services | Windows engine development in progress |
| Sprout Arcade | Native-game collection and eventual package distribution | First three local games committed; distribution planned later |
| Sprout Studio | Game development and validation tools | Planned later |
| Sprout Server | Optional sync, family management, and recommendations | Planned later |
| Sprout SDK | Schemas, package contracts, and developer tooling | Planned as real consumers emerge |

The initial **Sprout Family Launcher MVP** remains blocked on its development-card evidence: Onion launch, GameSwitcher return, lifecycle enforcement, safe boot, and device rendering are unverified. Its exact halt point is [documented and tagged](docs/development/launcher-mvp-halt.md). Independent Windows work is now growing the runtime through the first three concrete Arcade games without claiming device compatibility. Catalogue hosting, downloads, signing infrastructure, and device support remain deferred. See [Sprout Arcade](docs/arcade/README.md) and the [roadmap](docs/roadmap.md).

## Repository layout

```text
launcher/   Portable launcher state and the SDL2 desktop preview
runtime/    Native-game runtime (created by the Windows preview milestone)
games/      Concrete local Sprout Arcade packages used by the runtime
docs/       Architecture, specifications, research, and roadmap
tools/      Focused development entry points
.github/    Contribution templates
```

Server and connector directories remain intentionally absent until a concrete milestone needs them. Runtime and game-package structure is added only for the Windows preview's concrete game consumer.

## Development

Start with the [getting-started guide](docs/development/getting-started.md), [Sprout Arcade guide](docs/arcade/README.md), [runtime guide](docs/runtime/README.md), [architecture overview](docs/architecture/overview.md), and relevant [architecture decisions](docs/decisions/).

On Windows with Visual Studio C++ tools:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\dev.ps1 -Action test
powershell -ExecutionPolicy Bypass -File .\tools\dev.ps1 -Action run
powershell -ExecutionPolicy Bypass -File .\tools\dev.ps1 -Action run-arcade
powershell -ExecutionPolicy Bypass -File .\tools\dev.ps1 -Action arcade-smoke
```

The long-form product direction is preserved in the [platform blueprint](docs/reference/Sprout_Platform_Design_and_Technical_Blueprint.md). The concise documents under `docs/` define the navigable working foundation.

## Legal

Sprout is licensed under the [GNU General Public License version 3](LICENSE).

Sprout does not distribute ROMs, proprietary BIOS files, copyrighted game artwork, or copyrighted media. Users are responsible for supplying and using content lawfully. Onion and other third-party components retain their own licenses and notices.
