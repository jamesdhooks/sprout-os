# Architecture Overview

Sprout is a family console layer for retro handhelds and small computers. Its initial target is the Miyoo Mini Plus on an Onion-based SD card. It is not a general Linux desktop, emulator replacement, cloud requirement, media platform, or app store in the first milestone.

The [platform blueprint](../reference/Sprout_Platform_Design_and_Technical_Blueprint.md) contains the long-form direction. This document defines the boundaries contributors should use now.

## Components and status

```text
SproutOS (first committed milestone)
  Launcher ─ Profiles ─ Local policy ─ Library
      │                         │
      ├── Onion adapter ────────┴── Onion launch/GameSwitcher
      └── Local configuration, journal, and recovery

Sprout Runtime (planned)       Optional Sprout Server (planned)
  Native game API                Sync, remote grants, recommendations

Sprout Arcade / Studio / SDK (planned after concrete consumers)
```

The repository currently contains portable launcher navigation, profile/configuration persistence, parent access enforcement, deterministic local GB/SNES discovery and library views, a typed Onion launch adapter, and a Windows SDL2 preview. Child time-policy enforcement, persisted recent/favorite activity, GameSwitcher integration, hardware-validated Onion execution, and device rendering do not exist yet.

## Process responsibilities

The launcher owns visible navigation, profile selection, library presentation, parent authentication, and setup. Early implementations may keep profile, policy, catalogue, and journal responsibilities in one process plus a narrow policy supervisor. Separate daemons require a demonstrated lifecycle or isolation need.

The policy boundary must observe launches and resumes outside the launcher. It accounts for active use with monotonic time and returns expired sessions through the normal save-and-exit path.

The local device owns the authoritative offline state needed to launch, enforce policy, recover, and operate profiles. An optional server may synchronize journals, deliver authenticated grants, host packages, or compute recommendations; it cannot be required for basic device operation.

## Compatibility boundaries

### Onion

Sprout uses Onion's emulator configuration, RetroArch integration, save-state lifecycle, GameSwitcher, shortcuts, power handling, and device services where verified. All interaction passes through a narrow adapter. Onion-derived code remains isolated; the preferred order is reuse, wrap, narrow patch, then fork.

MainUI is a separate compatibility concern and is not assumed to be replaceable from source. Recovery must preserve a path to stock Onion. Exact launch and safe-boot behavior remains subject to [source and hardware investigation](../research/onion-integration.md).

The current adapter validates canonical ROM and launcher paths, maps only the pinned GB and SFC packages, and passes the ROM as a distinct process argument. Its [desktop contract](../specs/onion-launch-adapter.md) does not prove target-device launch, return, save, activity, or GameSwitcher behavior.

### Native runtime

Sprout Runtime is independently versioned from SproutOS. A future game package targets runtime APIs for graphics, input, audio, storage, timing, deterministic randomness, achievements, and events rather than device-specific APIs. The runtime is not part of the launcher MVP.

### Connectors

Core models see connector categories and capabilities, never provider-specific fields. Connectors own provider configuration, health checks, credentials, caching, and unsupported-feature behavior. The launcher remains functional when every connector is disabled.

### Packages

Future packages are immutable, versioned, integrity-checked units with declared compatibility and capabilities. Package installation, catalogues, and signing are deferred until the launcher vertical slice is reliable.

## Configuration and security

Configuration resolves in this order:

```text
platform defaults → household → device → profile → library item → temporary grant
```

SQLite holds transactional state; versioned JSON holds portable definitions. Secrets are referenced rather than embedded. Writes require validation, atomic activation, snapshots, and last-known-good recovery.

Child mode uses an allowlist enforced at launch and resume boundaries. This protects against casual bypass, not a skilled person with physical or SD-card access. Profile data, connector credentials, package capabilities, and the Onion adapter are separate security boundaries.
