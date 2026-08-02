# Local Configuration Schema

Status: **implemented schema v1 core**. The desktop first-run presentation is still in progress.

## Boundary

Versioned JSON stores portable household, device, profile-override, and setup-progress definitions. SQLite remains authoritative for profile identity and lifecycle. Secrets, PINs, activity, grants, saves, ROM paths, connector tokens, and image bytes are outside this document.

The active file is `config/sprout.json`; its prior validated revision is `config/sprout.last-good.json`. Exact device roots remain provisional until the Onion baseline is verified.

## Version 1 fields

| Field | Purpose |
| --- | --- |
| `schemaVersion` | Selects the parser and migration contract; v1 rejects other versions without modifying them |
| `revision` | Monotonic optimistic-concurrency revision |
| `nextSetupStep` | Last durable wizard boundary, or `complete` |
| `household` | Stable household ID and optional locale overrides |
| `device` | Stable device ID, offline-setup state, and optional locale overrides |
| `profiles` | Profile IDs with optional locale overrides; it does not duplicate profile identity data |
| `parentCredentialRef` | Optional opaque `secret:` reference; never PIN material or a hash |

Locale values resolve in this order:

```text
platform defaults → household → device → profile
```

Library-item overrides and temporary grants join the hierarchy only when their implementing issues establish concrete fields.

## Activation and recovery

The store validates the complete typed document before writing. It durably flushes a same-directory pending file and atomically replaces the destination. Before replacing an existing active file, it atomically snapshots that validated revision as last known good. Stale revisions are rejected so two writers cannot silently overwrite each other.

Parsing is strict: required types, duplicate keys, unknown keys, invalid UTF-8, invalid credential references, and unsupported schema versions fail closed. Schema v1 does not silently discard unknown data. Last-known-good restore validates the snapshot before activation and never snapshots a corrupt active file over it.

## Setup progress

The persisted sequence is welcome, locale, network/offline, first parent, optional parent PIN reference, optional child, avatars, library, child defaults, connectors, review, and complete. Only first-parent creation is mandatory. All other steps can be skipped, and completion is valid without network access or external services.

Profile creation is idempotent across interruption: if a matching profile row exists before its step is advanced, resume accepts it; a conflicting row with the same ID fails without advancing.
