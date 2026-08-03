# Launcher Configuration Recovery

Status: **implemented in the Windows desktop launcher; device boot integration unverified**.

After three consecutive unfinished data-backed starts, the launcher opens recovery before profile, policy, credential, or library stores. The recovery menu is controller-accessible and defaults to exit so no destructive action occurs from a single confirmation.

## Actions

- **Restore last-known-good:** offered only when the snapshot parses and validates. Confirmation previews its revision and next setup step, defaults to cancel, and activates the exact validated snapshot.
- **Reset launcher setup:** defaults to cancel, moves only the active `sprout.json` to `config/recovery/sprout.failed-startup-<attemptId>.json` without overwrite, then creates a fresh schema-v1 welcome configuration.
- **Exit to power off:** leaves the startup attempt unfinished.

Reset does not remove or rewrite profiles, managed images, policy usage, credentials, grants, ROMs, saves, states, imports, exports, backups, or the last-known-good snapshot. A missing active configuration can still be reset. A linked or irregular active path, linked recovery directory, existing quarantine target, invalid snapshot, or write failure is reported and fails without claiming readiness.

After a successful restore or reset, the launcher opens its ordinary persistent stores. Startup is marked ready only after the first ordinary setup or profile frame is rendered. The recovery menu itself never clears the failure evidence.

## Remaining device boundary

This flow does not install an Onion startup hook, define a physical safe-mode gesture, isolate a failing connector, or start stock Onion. Those require development-card evidence tracked by the [Onion device check](../development/onion-device-check.md) and the open safe-boot issue.
