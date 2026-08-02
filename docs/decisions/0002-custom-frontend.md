# ADR 0002: Build a custom Sprout frontend

- Status: Accepted
- Date: 2026-08-02

## Context

Profiles, family-safe navigation, policy visibility, resumable setup, and unified library items cannot be delivered credibly as a theme or a collection of MainUI menu patches.

## Decision

Sprout Launcher will own the visible family experience while Onion remains the initial lower-level emulation foundation. Child and parent capabilities are enforced at launch and resume boundaries, not only hidden in the interface.

## Consequences

The launcher must integrate with Onion lifecycle and recovery rather than bypass it. The first implementation is a narrow desktop and development-card vertical slice, not a full replacement for every Onion application.
