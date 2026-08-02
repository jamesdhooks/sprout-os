# ADR 0008: Use Argon2id and authenticated local grants

- Status: Accepted
- Date: 2026-08-02

## Context

The launcher needs an offline parent PIN, a reboot-persistent end-of-day unlock, immediate manual lock, and fail-closed behavior after clock rollback or storage tampering. Plain PINs, fast hashes, reversible encryption, and unsigned JSON do not meet that boundary. The design is not intended to resist an attacker who can read and modify both the SD card and Sprout binaries.

## Decision

Use the [official Argon2 reference implementation](https://github.com/P-H-C/phc-winner-argon2) release `20190702`, pinned to commit `62358ba2123abd17fccf2a108a301d4b52c01a7c` and built from its portable single-threaded sources. Store self-describing Argon2id hashes with a unique 16-byte operating-system-generated salt, a 32-byte output, 19 MiB of memory, two passes, and one lane. These parameters match the [current OWASP minimum](https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html); target-device latency and memory pressure must be measured before release.

Keep credentials and the active grant in a dedicated versioned SQLite store. Authenticate each grant with keyed BLAKE2b from the same pinned reference source and a 32-byte device-local random key stored separately from the database. The authenticated record covers its credential reference, issue time, last observed time, local date, and nonce. A date change, backward clock movement, invalid authentication code, malformed record, or manual lock removes the grant.

## Alternatives considered

- **Fast SHA-256 or a handwritten password scheme:** smaller, but unsuitable for low-entropy PIN storage and unnecessarily risky.
- **PBKDF2:** widely available, but less resistant to parallel guessing than the selected memory-hard design.
- **The complete libsodium library:** provides excellent high-level APIs, but introduces a substantially larger source and target-build surface when this milestone currently consumes only password hashing, random bytes, and a keyed authenticator.
- **Unsigned SQLite or JSON grants:** simpler, but permits casual extension or recreation of unlock state without parent authentication.

## Consequences

- Credential hashes encode their algorithm and work factors, allowing a later verified device profile to trigger rehashing without a schema redesign.
- Windows uses the system CNG random generator; the Onion/Linux path uses `/dev/urandom`. The latter and the Argon2 cost remain target-validation requirements.
- The added reference sources are dual-licensed CC0/Apache-2.0, portable C, and compile without thread or processor-specific optimized code. Release binary-size impact remains unmeasured.
- A device-local authentication key detects ordinary database tampering but does not create hardware-backed trust. An attacker with SD-card and binary access remains outside the documented parental-control threat model.
