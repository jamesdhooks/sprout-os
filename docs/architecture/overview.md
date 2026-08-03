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

Sprout Runtime (Windows preview) Optional Sprout Server (planned)
  Native game lifecycle          Sync, remote grants, recommendations

Local Arcade package (Windows preview)
Remote Arcade / Studio / SDK (planned after concrete consumers)
```

The repository currently contains portable launcher navigation, profile/configuration persistence, parent access enforcement, a versioned one-profile archive boundary, persisted daily child-time and startup-health decision cores, desktop configuration recovery, deterministic local GB/SNES and native-package discovery, library views, typed Onion and native-runtime launch adapters, and a Windows SDL2 preview. Persisted recent/favorite activity, GameSwitcher integration, live policy enforcement during an external process, hardware-validated Onion execution, safe boot to stock Onion, and device rendering do not exist yet.

## Process responsibilities

The launcher owns visible navigation, profile selection, library presentation, parent authentication, and setup. Early implementations may keep profile, policy, catalogue, and journal responsibilities in one process plus a narrow policy supervisor. Separate daemons require a demonstrated lifecycle or isolation need.

The policy boundary must observe launches and resumes outside the launcher. The implemented core accounts for explicit active intervals with monotonic time, persists daily usage and warnings, blocks expired starts, and requests normal save-and-exit. Hardware work must still connect verified Onion pause, resume, suspend, and return events to that boundary.

The local device owns the authoritative offline state needed to launch, enforce policy, recover, and operate profiles. An optional server may synchronize journals, deliver authenticated grants, host packages, or compute recommendations; it cannot be required for basic device operation.

## Compatibility boundaries

### Onion

Sprout uses Onion's emulator configuration, RetroArch integration, save-state lifecycle, GameSwitcher, shortcuts, power handling, and device services where verified. All interaction passes through a narrow adapter. Onion-derived code remains isolated; the preferred order is reuse, wrap, narrow patch, then fork.

MainUI is a separate compatibility concern and is not assumed to be replaceable from source. Recovery must preserve a path to stock Onion. Exact launch and safe-boot behavior remains subject to [source and hardware investigation](../research/onion-integration.md).

The current adapter validates canonical ROM and launcher paths, maps only the pinned GB and SFC packages, and passes the ROM as a distinct process argument. Its [desktop contract](../specs/onion-launch-adapter.md) does not prove target-device launch, return, save, activity, or GameSwitcher behavior.

### Native runtime

Sprout Runtime is independently versioned from SproutOS. The Windows preview establishes APIs exercised by one packaged microgame: deterministic stepping, action input, rendering, profile- and package-isolated local storage, structured events, and launcher handoff. The next phase grows reusable engine services through Mouse Maze, Blocks & Buttons, and Snake as described in the [runtime overview](../runtime/README.md). Platform integration and bounded deterministic services belong to the runtime; rules, content, difficulty parameters, and game-specific generation remain in packages and offline tools. Audio, broader SDK tooling, and additional platform adapters require concrete consumers. The runtime is not part of the launcher MVP, and Windows evidence does not establish device compatibility.

### Connectors

Core models see connector categories and capabilities, never provider-specific fields. Connectors own provider configuration, health checks, credentials, caching, and unsupported-feature behavior. The launcher remains functional when every connector is disabled.

### Packages

The Windows preview uses a local, versioned package with declared runtime compatibility and capabilities. Remote installation, catalogue services, downloads, signing infrastructure, updates, and rollback are deferred until the runtime has a stable concrete consumer and the package threat model is accepted.

## Configuration and security

Configuration resolves in this order:

```text
platform defaults → household → device → profile → library item → temporary grant
```

SQLite holds transactional state; versioned JSON holds portable definitions. The implemented one-profile archive carries only portable profile fields, a child allowance, and an explicitly selected normalized portrait; it excludes secrets, usage, saves, and library activity. Startup-health state is a separate clock-independent SQLite boundary and cannot mutate user data. Secrets are referenced rather than embedded. Writes require validation, atomic activation, snapshots, and last-known-good recovery.

Child mode uses an allowlist enforced at launch and resume boundaries. This protects against casual bypass, not a skilled person with physical or SD-card access. Profile data, connector credentials, package capabilities, and the Onion adapter are separate security boundaries.
