# ADR 0005: Use SQLite and versioned JSON for local state

- Status: Accepted
- Date: 2026-08-02

## Context

Profiles, grants, activity, and indexes need transactional updates, while household definitions and exports need portable, inspectable representations.

## Decision

SQLite will hold transactional state. Versioned JSON will hold portable configuration, manifests, and exports. Secrets are stored separately and referenced. Every active schema requires validation and an explicit migration path.

## Consequences

The split is by data behavior, not convenience. Writes must be atomic and recoverable. Exact tables and JSON shapes are deferred until the launcher persistence issue establishes real access patterns.
