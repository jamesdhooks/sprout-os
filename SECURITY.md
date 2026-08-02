# Security Policy

## Supported versions

Sprout has no released or supported versions. Security-sensitive design work on the active development branch is still welcome, but no current build should be treated as production-hardened.

Once the repository is published on GitHub, report vulnerabilities through GitHub's private vulnerability reporting for this repository. Do not disclose parental-control bypasses, credential exposure, package-verification flaws, or data-leakage issues in a public issue. A fallback contact will be added only when a real monitored address exists.

Include affected component and revision, reproduction steps, impact, required access, and any suggested mitigation. Do not include ROMs, personal profile images, credentials, or other private household data.

## Security boundaries

Sprout parental controls are intended to prevent accidental or casual bypass by children. They are not designed to resist a skilled attacker with physical possession of the device, SD-card access, custom firmware knowledge, or the ability to replace binaries or storage.

Security-sensitive design requirements include:

- child mode uses enforceable allowlists rather than hidden menu entries;
- policy enforcement must cover launched applications and resumes, not only launcher navigation;
- local PINs are stored with an appropriate password hash;
- persistent grants and packages are authenticated before use;
- package installation verifies signatures, hashes, compatibility, and rollback state;
- connector credentials live in a secret store and ordinary configuration contains references only;
- connectors receive only declared capabilities and per-profile access;
- native games cannot access shell execution, raw credentials, unrestricted filesystems, or another profile's data;
- configuration and policy writes are validated, atomic, recoverable, and backed by a last-known-good state.

Network integrations are optional. Core profile selection, local parental controls, launch policy, and recovery must not fail open when a server or connector is unavailable.

Physical safe mode is a recovery boundary, not a parental-control security boundary. Device loss, malicious SD-card modification, and compromised host computers remain outside the initial threat model.
