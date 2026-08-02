# Profile Model

Status: **provisional v0.1**. This is a domain contract for the launcher MVP, not a final database or wire schema.

## Fields

| Field | Purpose | v0.1 expectation |
| --- | --- | --- |
| `schemaVersion` | Selects validation and migration rules | Required integer |
| `id` | Stable profile identity | Required, immutable, locally unique |
| `displayName` | User-visible name | Required; validation limits unresolved |
| `role` | Selects policy defaults and administrative capabilities | At minimum `parent` and `child` |
| `avatarRef` | References a built-in avatar or managed local image | Optional until setup assigns one |
| `saveNamespace` | Isolates compatible game and application data | Required and immutable after data exists |
| `contentPolicyRef` | References the effective content allowlist/rules | Required for child profiles |
| `timePolicyRef` | References daily/session rules | Required for child profiles |
| `preferences` | Theme, accessibility, presentation, and difficulty choices | Profile scoped; structure evolves by schema version |
| `sync` | Optional synchronization identity and revision metadata | Disabled by default |
| `lifecycle` | Controls visibility and retention | `active`, `archived`, or `pending-deletion` |
| `createdAt` / `updatedAt` | Audit and conflict metadata | Required timestamps once persistence exists |

PIN hashes, connector credentials, active grants, play history, favorites, recents, and save data are separate records. They must not be embedded in a portable profile definition by default.

## Invariants

- A household must retain at least one active parent administrator.
- Child profiles receive conservative content and time defaults.
- Archive is reversible and does not delete saves, history, or images.
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
