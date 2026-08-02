# Sprout

Sprout is an open-source, family-oriented gaming platform for retro handhelds and small computers. It aims to provide family profiles, parental controls, an Onion-compatible emulation foundation, and a portable native-game runtime without making basic use depend on a server.

> **Status: early development.** This repository currently contains the project foundation and design specifications. There is no runnable Sprout build yet.

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
| Sprout Runtime | Portable native-game execution | Planned after the launcher MVP |
| Sprout Arcade | Native-game catalogue and package distribution | Planned later |
| Sprout Studio | Game development and validation tools | Planned later |
| Sprout Server | Optional sync, family management, and recommendations | Planned later |
| Sprout SDK | Schemas, package contracts, and developer tooling | Planned as real consumers emerge |

The initial milestone is the **Sprout Family Launcher MVP**: a development-card vertical slice from profile selection through Onion launch, GameSwitcher return, time accounting, parent control, and safe recovery. See the [roadmap](docs/roadmap.md).

## Repository layout

```text
docs/       Architecture, specifications, research, and roadmap
.github/    Contribution templates
```

Product directories will be added only when implementation begins. Empty launcher, runtime, server, and connector skeletons are intentionally absent.

## Development

Start with the [getting-started guide](docs/development/getting-started.md), [architecture overview](docs/architecture/overview.md), and relevant [architecture decisions](docs/decisions/). There is currently no build command or executable target.

The long-form product direction is preserved in the [platform blueprint](docs/reference/Sprout_Platform_Design_and_Technical_Blueprint.md). The concise documents under `docs/` define the navigable working foundation.

## Legal

Sprout is licensed under the [GNU General Public License version 3](LICENSE).

Sprout does not distribute ROMs, proprietary BIOS files, copyrighted game artwork, or copyrighted media. Users are responsible for supplying and using content lawfully. Onion and other third-party components retain their own licenses and notices.
