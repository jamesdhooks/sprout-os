# ADR 0003: Separate Sprout Runtime from SproutOS

- Status: Accepted
- Date: 2026-08-02

## Context

Native Sprout games need a portable lifecycle and release cadence that does not require rebuilding the handheld distribution.

## Decision

Sprout Runtime will be an independently versioned execution boundary for native games. SproutOS launches and governs runtime packages but does not embed game-specific APIs into the launcher.

## Consequences

Runtime, SDK, Studio, and package work is deferred until the launcher vertical slice is reliable. No empty runtime package or placeholder API is created at bootstrap.
