# ADR 0001: Use Onion as the initial emulation foundation

- Status: Accepted
- Date: 2026-08-02

## Context

The Miyoo Mini Plus requires reliable emulator configuration, save-state lifecycle, GameSwitcher integration, input shortcuts, power handling, and device services. Rebuilding these before validating Sprout's family experience would add risk without user value.

## Decision

SproutOS will initially run on an Onion-based development card and interact through an isolated Onion adapter. The order of preference is reuse unchanged, wrap, patch narrowly, then fork only with evidence. Physical behavior is accepted only after testing against a pinned Onion version.

## Consequences

Onion compatibility is a first-milestone constraint. Onion-derived changes remain isolated with their license notices. Closed-source MainUI behavior and hardware details cannot be assumed from desktop tests. A stock-Onion recovery route is mandatory.
