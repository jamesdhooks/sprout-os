# ADR 0005: Use SQLite and versioned JSON for local state

- Status: Accepted
- Date: 2026-08-02

## Context

Profiles, grants, activity, and indexes need transactional updates, while household definitions and exports need portable, inspectable representations.

## Decision

SQLite will hold transactional state. Versioned JSON will hold portable configuration, manifests, and exports. Secrets are stored separately and referenced. Every active schema requires validation and an explicit migration path.

## Consequences

The split is by data behavior, not convenience. Writes must be atomic and recoverable. Exact tables and JSON shapes are added only as launcher issues establish real access patterns.

The first concrete consumer is profile persistence. It embeds the official SQLite 3.53.4 amalgamation, pinned by SHA3-256, as a static C library. This avoids requiring a system package that is absent from a normal Windows development setup and not yet defined for the Onion toolchain. The alternatives were a platform-provided SQLite library, which would make host and device builds diverge, and ad hoc JSON files, which cannot provide the required transactional invariant and migration behavior.

SQLite is actively maintained and released to the public domain. The amalgamation source archive is about 2.8 MiB; final release-binary and device-storage impact remain unverified until the cross-compilation toolchain exists. Its C implementation is portable to the intended target in principle, but target compatibility must be demonstrated on the development card before a device claim is made.

The first JSON consumer uses yyjson 0.12.0 pinned to commit `7871d321ff4cd8068c1f777c97975dc2fb640ab3`. It is an MIT-licensed, actively maintained, portable C parser with MSVC and ARM coverage. A small C library was preferred over handwritten parsing, which is unsafe for recovery data, and a large C++ DOM dependency, which would increase compile and target cost. Non-standard JSON and fast floating-point conversion are disabled because schema v1 uses strict JSON and no floating-point values. Final target binary impact and Onion compatibility remain unverified.
