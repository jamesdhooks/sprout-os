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

For the current Windows desktop launcher slice, run:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\dev.ps1 -Action test
```

Reviewers should check for scope expansion, duplicate abstractions, unused code, hidden server dependencies, unsafe migrations, missing rollback behavior, profile-data leakage, parental-control bypasses, inaccurate documentation, and files without a concrete consumer.

Update user or developer documentation whenever behavior, configuration, migration, or compatibility changes. Use current, planned, and exploratory language precisely.

## Packages and future connectors

The Windows preview accepts only checked-in local packages following [Runtime Package v1](docs/specs/runtime-package-v1.md). A game contribution currently requires a milestone issue, original or compatibly licensed content, deterministic host tests, controller-only playability, declared capabilities, and an explicit license. Follow the [Arcade catalogue](docs/arcade/catalogue.md), [runtime engine ownership](docs/runtime/engine-systems.md), and [asset provenance](docs/arcade/assets.md): reuse established engine services, keep game-specific rules in the package/tool, and do not copy private fonts, storage codecs, host wrappers, or lifecycle frameworks into a game. Do not present a local preview directory as a signed or safely installable third-party package.

Remote distribution and external-service connectors are not implemented. Future packages must add content and integrity metadata when those accepted specifications exist. Future connectors must declare provider-neutral capabilities, configuration requirements, credential references, health behavior, offline behavior, and per-profile permissions.

Do not add distribution or connector frameworks before a milestone issue establishes a concrete consumer and threat model.

## Security

Do not report vulnerabilities in a public issue. Follow [SECURITY.md](SECURITY.md).
