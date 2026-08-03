# ADR 0010: Establish the Runtime with a Windows Arcade Preview

- Status: Accepted
- Date: 2026-08-03

## Context

The host-tested launcher foundation is substantial, while the remaining launcher MVP acceptance criteria require a physical Onion device and development SD card that are not currently available. Sprout Runtime and native games are independently portable, and Windows is a required target with deterministic automated verification.

Waiting for device access would prevent work on this independent boundary. Starting the complete catalogue and distribution system would introduce package security, hosting, update, and publishing decisions before a runtime package has a concrete consumer.

## Decision

Proceed with a bounded Sprout Arcade Windows preview alongside the blocked device-validation track. The preview must use one real local package and one complete microgame to establish the smallest runtime API needed for deterministic stepping, action input, rendering, local storage, structured events, and launcher handoff.

Sprout Runtime remains independently versioned from SproutOS. The preview may add `runtime/` and a single concrete game package because both have immediate consumers and tests. It must not add a remote catalogue, downloader, signing service, publishing workflow, speculative SDK layers, or claims of Linux, Raspberry Pi, or Miyoo compatibility.

## Consequences

- Windows behavior can be implemented and verified without weakening hardware acceptance criteria.
- Runtime APIs are introduced only when exercised by the first game.
- Package validation begins locally, but trust and distribution remain separate future decisions.
- The launcher MVP remains blocked until its open device issues are demonstrated on hardware.
- Portable and device targets will require later validation and may expose changes to the Windows-established boundary.
