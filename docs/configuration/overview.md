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

Profile creation supports built-in avatars and local image import. Imported images remain local by default, preserve the source when requested, respect orientation, and generate only required crop/thumbnail variants. Upload to a connector requires explicit consent. Archive and delete are separate lifecycle operations; permanent deletion requires a second confirmation.

## Storage, migration, and recovery

SQLite is intended for transactional profile, activity, grant, and index state. Versioned JSON is intended for portable household, device, connector, library, and export definitions. Schemas require stable IDs, documented defaults, validation, migrations, and descriptive failures. Unknown-field preservation is preferred where safe but remains unresolved per schema.

Configuration is validated before activation, written atomically, and snapshotted. Repeated startup failure or invalid configuration must offer last-known-good rollback, connector isolation, launcher-only reset, and stock-Onion recovery without deleting saves.

The implemented schema-v1 core persists setup progress and household/device/profile locale overrides with strict validation, atomic activation, stale-writer protection, and last-known-good restore. The desktop preview now presents that flow and creates built-in parent/child profiles offline. See the [local configuration specification](../specs/local-configuration.md). Custom image import, secure PIN creation, connector isolation, launcher reset, and boot-failure recovery remain planned.

## Backup and restore

- **Household export:** profiles, policies, library metadata, connector definitions without secrets, preferences, and favorites.
- **Profile export:** one profile's portable configuration and selected progress data.
- **Full encrypted backup:** may include saves, states, profile images, credentials, and history when explicitly selected.

Archives require a versioned manifest, checksums, validation before restore, and a local no-service-required path. Partial restore must identify conflicts before modifying active state.

## Connectors and secrets

Connector configuration declares category, provider adapter, endpoint, credential reference, capabilities, health, cache behavior, and per-profile access. Core code depends on capabilities rather than provider names.

Credentials never appear directly in ordinary JSON, exports, logs, or issue reports. The future secret store must support revocation, status reporting, controlled backup, and short-lived tokens. Offline, expired, unreachable, and unsupported states are explicit and cannot destabilize local launcher behavior.
