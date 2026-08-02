# Engineering Principles

These principles constrain implementation choices. The [architecture overview](overview.md) explains the system boundaries.

1. **Offline first.** Profiles, launch policy, time limits, and recovery work without a network.
2. **Provider neutral.** Core models depend on capabilities, not a preferred vendor or household service.
3. **Profile scoped.** Saves, history, preferences, policy, and permissions are isolated wherever the platform permits it.
4. **Capability based.** Packages and connectors receive explicit, minimal capabilities and degrade when a capability is absent.
5. **Deterministic runtime.** Native games use stable timing, input actions, storage, and random sources suitable for replay tests.
6. **Smallest viable abstraction.** Add an abstraction for an immediate problem and real consumers, not an imagined future.
7. **Explicit migrations.** Versioned data changes include validation, migration, compatibility, and failure behavior.
8. **Recoverability.** Atomic writes, backups, rollback, and safe-mode escape paths protect user data and device usability.
9. **Safe defaults.** Child profiles and disconnected systems default to the least surprising permitted behavior.
10. **Graceful degradation.** Optional servers, connectors, artwork, and advanced rendering may disappear without breaking core use.
11. **Reuse before rewrite.** Reuse Onion unchanged, then wrap, patch narrowly, and fork only with evidence.
12. **No hidden server dependency.** Local capabilities never silently require Sprout Server or another external service.
