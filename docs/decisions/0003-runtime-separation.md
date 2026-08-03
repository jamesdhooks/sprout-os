# ADR 0003: Separate Sprout Runtime from SproutOS

- Status: Accepted
- Date: 2026-08-02

## Context

Native Sprout games need a portable lifecycle and release cadence that does not require rebuilding the handheld distribution.

## Decision

Sprout Runtime will be an independently versioned execution boundary for native games. SproutOS launches and governs runtime packages but does not embed game-specific APIs into the launcher.

## Consequences

Runtime, SDK, Studio, and package work was deferred at bootstrap, so no empty runtime package or placeholder API was created. [ADR 0010](0010-windows-arcade-preview.md) permits a bounded Windows runtime preview now that a real local package and microgame provide concrete consumers; distribution and speculative SDK work remain deferred.
