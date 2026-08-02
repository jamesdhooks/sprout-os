# Contributing to Sprout

Sprout is in early development. Contributions should advance the current milestone without implying that planned systems already exist.

## Before starting

1. Read the [architecture overview](docs/architecture/overview.md), [engineering principles](docs/architecture/principles.md), and relevant [ADRs](docs/decisions/).
2. Select an issue with explicit acceptance criteria and dependencies.
3. Confirm that the proposed change belongs to the active milestone.
4. Inspect existing code and documentation before introducing a new structure or dependency.

For material architecture changes, open a focused proposal describing the immediate problem, alternatives, tradeoffs, migration path, and rollback. Record an accepted decision as an ADR; do not use ADRs for routine implementation choices.

## Workflow

- Use a product-focused branch such as `feat/profile-storage`, `fix/onion-launch-return`, or `docs/runtime-specification`.
- Keep each commit understandable and scoped. Use concise messages such as `feat: add profile persistence schema` or `test: cover configuration migrations`.
- Do not mix unrelated formatting, dependency upgrades, architecture work, and feature changes.
- Preserve unrelated behavior and files, especially in a working tree with existing changes.
- Open a pull request that explains the problem, approach, verification, risks, and documentation impact.

## Implementation expectations

- Make the smallest coherent change that meets the issue acceptance criteria.
- Prefer reuse, wrapping, and narrow adapters over forks or broad rewrites.
- Add a dependency only with a documented purpose, alternatives, maintenance status, license, size impact, and target-platform compatibility.
- Create a shared abstraction only after at least two real consumers demonstrate a stable common contract.
- Avoid placeholder interfaces, unused modules, generic dumping grounds, TODO sprawl, and speculative compatibility layers.
- Treat configuration changes as versioned data changes with validation, migration, recovery, and unknown-field behavior considered explicitly.
- Keep secrets, private content, ROMs, BIOS files, local work logs, and device data out of Git.

## Testing and review

Changes must include the narrowest relevant tests and the exact commands and outcomes in the pull request. Distinguish unit, integration, desktop, and physical-hardware verification. A desktop simulation is not evidence of Miyoo behavior.

Reviewers should check for scope expansion, duplicate abstractions, unused code, hidden server dependencies, unsafe migrations, missing rollback behavior, profile-data leakage, parental-control bypasses, inaccurate documentation, and files without a concrete consumer.

Update user or developer documentation whenever behavior, configuration, migration, or compatibility changes. Use current, planned, and exploratory language precisely.

## Future packages and connectors

Native game packages and external-service connectors are not implemented yet. When their accepted specifications and validation tools exist:

- game packages must declare identity, version, runtime compatibility, capabilities, content metadata, assets, licenses, and integrity information;
- connectors must declare provider-neutral capabilities, configuration requirements, credential references, health behavior, offline behavior, and per-profile permissions.

Do not add a package or connector framework before a milestone issue establishes its first concrete consumer.

## Security

Do not report vulnerabilities in a public issue. Follow [SECURITY.md](SECURITY.md).
