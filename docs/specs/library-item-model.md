# Library Item Model

Status: **provisional v0.1**. The model describes launchable catalogue entries without requiring every content type to be implemented.

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

## Policy behavior

Effective visibility and launch permission are evaluated for the active profile at display, launch, and resume. Missing metadata never broadens child access. A disabled connector, missing package, missing ROM, or incompatible runtime produces an unavailable item with a recoverable explanation rather than a raw filesystem error.

## Versioning questions

Exact serialized unions, metadata provenance priority, artwork cache keys, ROM identity strategy, and collection query syntax remain unresolved until the launcher index has real consumers.
