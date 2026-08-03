# Profile Model

Status: **implemented profile schema v1 on database schema v2**. The broader profile contract remains provisional where noted.

## Fields

| Field | Purpose | v0.1 expectation |
| --- | --- | --- |
| `schemaVersion` | Selects validation and migration rules | Required integer |
| `id` | Stable profile identity | Required, immutable, locally unique |
| `displayName` | User-visible name | Required; validation limits unresolved |
| `role` | Selects policy defaults and administrative capabilities | At minimum `parent` and `child` |
| `avatarRef` | References a built-in avatar or managed local image | Optional until setup assigns one |
| `backgroundRef` | References a reviewed built-in home scene | Defaults to `builtin:garden-morning`; parent-managed |
| `saveNamespace` | Isolates compatible game and application data | Required and immutable after data exists |
| `contentPolicyRef` | References the effective content allowlist/rules | Required for child profiles |
| `timePolicyRef` | References daily/session rules | Required for child profiles |
| `preferences` | Theme, accessibility, presentation, and difficulty choices | Profile scoped; structure evolves by schema version |
| `sync` | Optional synchronization identity and revision metadata | Disabled by default |
| `lifecycle` | Controls visibility and retention | v1 persists `active` or `archived`; permanent-deletion workflow is deferred |
| `createdAt` / `updatedAt` | Audit and conflict metadata | Required timestamps once persistence exists |

PIN hashes, connector credentials, active grants, play history, favorites, recents, and save data are separate records. They must not be embedded in a portable profile definition by default.

Daily allowance and usage are likewise separate profile-scoped records. The implemented boundary is described in the [daily time-policy specification](daily-time-policy.md). First-run child creation persists the documented 45-minute default when a policy store is available.

## Persistence

The launcher implements profile records at schema version 1 in a SQLite `profiles` table. Database schema version 2 adds `backgroundRef` without changing the portable profile identity contract. Each row stores the fields above except optional synchronization identity and conflict state, which have no current consumer. `localRevision` advances on lifecycle, avatar, and background changes. Profile preferences are validated JSON but remain an empty object until a preference feature establishes concrete keys.

Opening an empty version-zero database creates database schema version 2 inside one transaction. Opening version 1 adds the background reference with the safe garden default. A database with a newer version is rejected without modification. A failed migration rolls back its schema and version changes. There is no destructive migration or permanent-delete operation.

Built-in avatars use `builtin:<id>` references validated against the packaged
64-item catalogue. Authenticated Profile Settings can replace the reference for
any active profile. The implemented import core uses revisioned `local:<id>`
references for managed PNG variants; image bytes and source filesystem paths
are not stored in the profile row. See the [profile image pipeline](profile-images.md).

Built-in backgrounds use validated `builtin:<id>` references. Authenticated
Profile Settings presents Profile Image and Home Background as separate choices.
The initial catalogue contains Garden Morning, Firefly Evening, Sunny Cove, and
Treehouse Library. The profile-select screen always uses the neutral household
scene; the active profile's choice is applied after selection.

The implemented [portable profile archive](profile-archive.md) exports one active profile with its selected background, concrete child allowance, and optional normalized managed avatar. Older archives without `backgroundRef` restore to the safe garden default. It intentionally omits lifecycle history, local revisions, timestamps, credentials, grants, usage, saves, and library activity. Restore accepts only a new identity and save namespace; schema v1 does not merge or remap.

## Invariants

- A household must retain at least one active parent administrator.
- Child profiles receive conservative content and time defaults.
- Archive is reversible and does not delete profile fields, saves, history, or images.
- Permanent deletion requires a separate authenticated operation and an explicit retention decision.
- Profile switching cannot expose another profile's policy, history, native saves, or connector data.
- Emulator isolation is enabled only for paths and platforms proven compatible with Onion's lifecycle.

## Synchronization

Synchronization is optional. Metadata should identify the local revision, last acknowledged remote revision, and conflict state without making the remote authoritative for offline launch or policy. Conflict rules are unresolved until a real sync consumer exists.

## Open questions

- Whether `teen`, `adult`, and `guest` are persisted roles or presets over capabilities.
- Whether age data is stored as a birth date, age band, or not at all.
- Which Onion save, state, remap, and GameSwitcher paths can be switched safely per profile.
- Display-name limits and normalization rules.
- Unknown-field preservation rules for each serialized representation.
