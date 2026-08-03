# Changelog

All notable user-visible changes to Sprout will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and released versions will follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html) once versioned software exists.

## [Unreleased]

### Added

- A controller-accessible, parent-reauthenticated one-profile export and restore flow with versioned checksummed archives, conflict preflight, explicit unencrypted-portrait consent, and no secrets, usage, saves, or library activity.
- A profile-scoped daily-time-policy core with monotonic active accounting, persisted usage and one-time warnings, bounded restart recovery, rollback checks, and explicit launch-block/save-and-exit decisions.
- A reproducible Onion ARM cross-build, pinned compiler/sysroot and CMake inputs, CI coverage, isolated device diagnostic, and development-card validation protocol.
- Deterministic local Onion GB/SNES discovery plus controller-driven recent, favorite, and all-game views with fail-closed unavailable states.
- A typed Onion GB/SNES launch adapter with canonical path, policy, extension, launcher, and structured process-outcome validation, plus desktop contract tests.
- Controller-based parent PIN setup and entry, Argon2id storage, authenticated reboot-persistent end-of-day grants, sensitive-action reauthentication, manual lock, and clock-rollback checks.
- Local parent-profile image import with controller-driven cropping, versioned managed PNG variants, metadata removal, safe replacement, and persisted portrait rendering in the desktop preview.
- Resumable offline first-run setup with atomic versioned configuration, last-known-good recovery, built-in parent/child profiles, and a 640×480 desktop presentation.
- Versioned SQLite profile persistence with parent/child validation, reversible archive and restore, last-active-parent protection, and migration rollback tests.
- Initial 640×480 desktop launcher preview with deterministic parent/child fixtures, action-level keyboard and controller input, and navigation/render smoke tests.
