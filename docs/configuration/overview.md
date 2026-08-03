# Configuration Overview

Sprout configuration must be manageable through product surfaces rather than required file editing. The on-device UI is authoritative for essential setup; a future desktop or local-web manager and portable archives may use the same schemas and validation.

## Setup and precedence

The first-run wizard is resumable and offline-capable. It collects language/region/time zone, optional networking, the first parent and PIN, household profiles and avatars, local library roots, conservative child defaults, and optional connectors. Only creation of a parent administrator is mandatory.

Values resolve from least to most specific:

```text
platform defaults → household → device → profile → item override → temporary grant
```

The UI should show inherited and overridden values. A temporary grant never rewrites the underlying policy.

## Intended local layout

```text
/Sprout/
  config/       Versioned portable definitions
  data/         SQLite state, profiles, packages, and caches
  imports/      Friendly import locations
  exports/      Portable exports
  backups/      Local recoverable backups
  logs/         Bounded diagnostic logs
```

Exact device paths are provisional until Onion integration and storage behavior are verified. Application updates must not overwrite `data/`, exports, backups, or user-supplied content.

## Profiles and images

Profile creation supports a packaged collection of 32 built-in avatars and local image import. The first-run portrait step can assign built-ins to parent and child profiles. After setup, freshly authenticated parents use Profile Settings to select any active profile and assign a built-in or staged custom image. Imported images remain local by default, respect orientation, and generate only required crop/thumbnail variants; source paths are not retained. Upload to a connector requires explicit consent. Archive and delete are separate lifecycle operations; permanent deletion requires a second confirmation.

## Storage, migration, and recovery

SQLite is intended for transactional profile, activity, grant, and index state. Versioned JSON is intended for portable household, device, connector, library, and export definitions. Schemas require stable IDs, documented defaults, validation, migrations, and descriptive failures. Unknown-field preservation is preferred where safe but remains unresolved per schema.

Configuration is validated before activation, written atomically, and snapshotted. After three unfinished starts, the desktop launcher presents validated last-known-good restore or launcher-only reset before opening ordinary user stores. Reset quarantines only the active configuration and preserves user data. See the [startup-health](../specs/startup-health.md) and [launcher recovery](../specs/launcher-recovery.md) specifications. Connector isolation and stock-Onion startup remain planned.

The implemented schema-v1 core persists setup progress and household/device/profile locale overrides with strict validation, atomic activation, stale-writer protection, last-known-good restore, and collision-safe active-configuration quarantine. The desktop preview presents that flow, creates parent/child profiles offline, assigns the blueprint's 45-minute child allowance, exposes all 32 packaged portraits, can import and crop a staged custom image without persisting its source path, and stores only an opaque reference after secure PIN creation. Existing preview children without a concrete allowance receive that default idempotently at startup. See the [local configuration specification](../specs/local-configuration.md). Connector isolation and device boot recovery remain planned.

## Backup and restore

- **Profile export (implemented):** one active profile's portable configuration, concrete child allowance, and optional managed portrait. It excludes secrets, usage, saves, states, library activity, and source paths. See the [portable profile archive specification](../specs/profile-archive.md).
- **Household export (planned):** profiles, policies, library metadata, connector definitions without secrets, preferences, and favorites.
- **Full encrypted backup (planned):** may include saves, states, profile images, credentials, and history when explicitly selected.

The implemented profile archive has a versioned manifest, a BLAKE2b-256 payload checksum, strict validation, and a local no-service-required path. Restore identifies profile, save-namespace, policy, and image conflicts before mutation and does not merge or overwrite.

## Connectors and secrets

Connector configuration declares category, provider adapter, endpoint, credential reference, capabilities, health, cache behavior, and per-profile access. Core code depends on capabilities rather than provider names.

Credentials never appear directly in ordinary JSON, exports, logs, or issue reports. The future secret store must support revocation, status reporting, controlled backup, and short-lived tokens. Offline, expired, unreachable, and unsupported states are explicit and cannot destabilize local launcher behavior.
