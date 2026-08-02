# Library Item Model

Status: **emulated-game v1 subset implemented; broader model provisional**. The model describes launchable catalogue entries without requiring every content type to be implemented.

```text
LibraryItem
├── EmulatedGame
├── NativeGame
├── MediaApplication
├── UtilityApplication
└── ContentCollection
```

## Common fields

| Group | Fields and expectations |
| --- | --- |
| Identity | Stable `id`, `type`, and `schemaVersion` |
| Metadata | `title`, optional summary, platform/category, players, and locale-aware values where available |
| Launch | Typed `launchTarget`; never an arbitrary child-visible shell command |
| Artwork | Managed references for icon, cover, screenshots, and provenance |
| Classification | Tags, content rating/age guidance when known, and source |
| Visibility | Default visibility plus explicit profile or policy overrides |
| Permissions | Required device/runtime/connector capabilities |
| Versioning | Item revision and source revision; native packages also carry package/runtime versions |

## Item-specific targets

- **EmulatedGame:** ROM reference, Onion system identifier, and verified Onion launch definition.
- **NativeGame:** signed package identity and runtime compatibility.
- **MediaApplication:** application or connector-backed media entry point with declared capabilities.
- **UtilityApplication:** allowlisted local application target and administrative classification.
- **ContentCollection:** stable ordered or query-backed references to other items; it is not directly executable.

Paths and credentials are not portable identity. A ROM fingerprint may assist local reconciliation, but Sprout does not distribute or upload ROM content.

## Implemented emulated-game subset

The local scanner currently discovers regular files beneath the pinned Onion `Roms/GB` and `Roms/SFC` roots. It canonicalizes every candidate, rejects paths outside the system root, filters using the pinned package extensions, and returns deterministic case-insensitive title order. It does not inspect archives or file contents.

Version 1 item identity is `onion:<system>:<percent-encoded-relative-path>`. This is stable across repeated scans of the same layout and avoids exposing an executable command. Moving or renaming a ROM changes the identity; content-based reconciliation remains unresolved.

The controller presentation filters concrete entries into recent, favorite, and all-game views and emits an `EmulatedLaunchTarget` only for an allowed selection. The desktop preview uses sanitized in-memory recent/favorite metadata. Discovered child items fail closed until an explicit profile allowlist exists; parent selections can emit typed preview requests but Windows does not execute Onion scripts.

## Policy behavior

Effective visibility and launch permission are evaluated for the active profile at display, launch, and resume. Missing metadata never broadens child access. A disabled connector, missing package, missing ROM, or incompatible runtime produces an unavailable item with a recoverable explanation rather than a raw filesystem error.

## Versioning questions

Exact serialized unions beyond emulated games, metadata provenance priority, persisted recent/favorite storage, artwork cache keys, content-based ROM reconciliation, and collection query syntax remain unresolved until they have real consumers.
