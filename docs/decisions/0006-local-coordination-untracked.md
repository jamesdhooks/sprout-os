# ADR 0006: Keep implementation coordination untracked

- Status: Accepted
- Date: 2026-08-02

## Context

Task ledgers, scratch decisions, raw research, and handoff notes help implementation but are not useful public project documentation. Tracking them creates duplicated status and exposes process debris.

## Decision

Detailed coordination lives in a sibling local workspace outside the repository. Public progress is represented by the roadmap, changelog, issues, milestones, commits, and pull requests.

## Consequences

Public documents must stand on their own for ordinary contributors. Durable verified findings move into maintained documentation or ADRs; internal narrative, prompts, and session logs do not.
