# Changelog

All notable user-visible changes to Sprout will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and released versions will follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html) once versioned software exists.

## [Unreleased]

### Added

- Local parent-profile image import with controller-driven cropping, versioned managed PNG variants, metadata removal, safe replacement, and persisted portrait rendering in the desktop preview.
- Resumable offline first-run setup with atomic versioned configuration, last-known-good recovery, built-in parent/child profiles, and a 640×480 desktop presentation.
- Versioned SQLite profile persistence with parent/child validation, reversible archive and restore, last-active-parent protection, and migration rollback tests.
- Initial 640×480 desktop launcher preview with deterministic parent/child fixtures, action-level keyboard and controller input, and navigation/render smoke tests.
