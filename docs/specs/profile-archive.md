# Portable Profile Archive

Status: **implemented schema v1 for the desktop launcher and ARM diagnostic; device UI verification pending**.

## Boundary

A `.sprout-profile` file carries one active profile between Sprout installations without a server. Export and restore are available from the PIN-reauthenticated parent menu. The operation is deliberately narrower than a household backup.

Schema v1 includes:

- stable profile ID, display name, role, and save namespace;
- content- and time-policy references;
- validated profile preferences JSON;
- the selected built-in home-background reference;
- one concrete daily allowance for a child profile; and
- either a built-in avatar reference or, after explicit consent, the managed portrait and thumbnail PNGs.

It excludes PIN hashes, credential references, unlock grants, connector secrets, usage history, warnings, active sessions, saves, save states, ROM paths, favorites, recents, activity logs, and source-image paths. A managed portrait is personal data and is stored unencrypted; the UI defaults its consent screen to Cancel.

## Envelope and integrity

The outer JSON object is strict and contains:

| Field | Schema-v1 value |
| --- | --- |
| `archiveKind` | `sprout-profile` |
| `schemaVersion` | `1` |
| `checksumAlgorithm` | `blake2b-256` |
| `payloadEncoding` | `base64` |
| `payloadChecksum` | Lowercase checksum of the decoded payload |
| `payload` | Base64-encoded canonical JSON payload |

The payload contains its own schema version, `profile`, and `dailyAllowanceSeconds`. The profile contains the portable fields above and an `avatar` object whose kind is `builtin` or `managedPng`. `backgroundRef` is an additive schema-v1 field: readers accept an older archive without it and restore the safe Garden Morning default. Unknown or duplicate fields, malformed Base64, unsupported versions, noncanonical identifiers, oversized input, and checksum mismatches fail before restore.

The current caps are 8 MiB per archive, 6 MiB decoded payload, 1 MiB portrait, and 256 KiB thumbnail. Embedded images must have a PNG signature and valid IHDR dimensions matching the managed 256×256 portrait and 96×96 thumbnail contract.

## Export and restore behavior

Export accepts only an explicit new `.sprout-profile` path in an existing directory. It writes and synchronizes an exclusively created sibling pending file, then uses the platform's atomic no-replace rename (`MoveFileExW` or Linux `renameat2(RENAME_NOREPLACE)`). It never overwrites an archive and removes the pending file when activation fails. Target-kernel and SD-filesystem support for the Linux operation remains part of device validation. The desktop flow uses `exports/profile-<hex-profile-id>.sprout-profile` so profile IDs cannot become paths.

Restore first validates the complete archive and checks for an existing profile ID, save namespace, daily-policy row, or managed-image generation. A conflict changes nothing. For a valid new profile, restore writes any image generation, then the unused policy row, and creates the profile last; pre-profile failures are compensated. Restored image paths are derived locally from the profile ID and archive checksum, never from an archive-supplied path.

The desktop restore browser reads only regular `.sprout-profile` files directly inside `imports/`, ignores invalid entries, and sorts candidates by filename. A successful restore reloads the profile selector. Archive merge, ID remapping, save transfer, encrypted household backup, and overwrite are non-goals for schema v1.

## Compatibility

Readers reject newer envelope or payload schema versions without modification. Any future schema change requires an explicit migration or a new reader path; schema-v1 writers do not silently preserve unknown fields.
