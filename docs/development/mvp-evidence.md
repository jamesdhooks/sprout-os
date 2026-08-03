# Launcher MVP Evidence Checklist

This checklist defines the evidence required before the Sprout Family Launcher MVP can be called reliable. Passing host tests is necessary but cannot substitute for development-card behavior. Do not mark a hardware-only row complete from source review, emulation, cross-compilation, or desktop screenshots.

## Pinned baseline

Record the Sprout commit and use:

- Onion `v4.3.1-1`, upstream commit `7dfc008b851398dcfe57819519efe5f958c77f65`;
- Onion toolchain manifest `sha256:a8da1021449c80c0ccb75e263f1dfc75b5a004278fefa8a54151e55698a352f4`;
- GCC 8.3.0 `arm-linux-gnueabihf` with the glibc 2.28 sysroot; and
- the dependency revisions in [Getting Started](getting-started.md).

Stop and reconcile the evidence if the tested device, firmware, Onion version, toolchain manifest, or Sprout commit differs. Never silently treat a nearby version as equivalent.

## Evidence classes

- **Automated:** deterministic host or cross-build checks run from a clean checkout.
- **Desktop manual:** visible 640×480 preview behavior checked with sanitized local fixtures.
- **Hardware only:** behavior that must be observed on a dedicated development card and target device.

## Automated gates

Run from the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\dev.ps1 -Action test
powershell -ExecutionPolicy Bypass -File .\tools\build-onion.ps1 -Action build
```

| Gate | Required evidence |
| --- | --- |
| Interrupted setup and profile reload | `mvp-host-integration` passes after reopening the shared configuration, profile, policy, and startup-health paths |
| GB recent and SNES favorite | The integration journey discovers synthetic empty files and records one validated argument for each fixed Onion launcher; no script or ROM content executes |
| Child time expiry | The journey reports `save_and_exit_required`, denies continued launch, reopens the policy store, and still blocks another launch |
| Parent unlock and relock | A persisted end-of-day grant enters parent mode and manual lock immediately revokes it |
| Profile portability | A child profile and allowance restore into clean stores while an out-of-scope save sentinel remains unchanged |
| Repeated-start recovery | Three unfinished attempts route the fourth to validated recovery; readiness clears failures only after the recovery action |
| Migrations and corruption | Focused configuration, profile, access, policy, archive, and startup-health tests reject newer or invalid state without adoption |
| Controller and rendering traversal | Presentation tests and `launcher-desktop-smoke` pass under SDL's dummy video/audio drivers |
| Onion-compatible compilation | The pinned ARM build produces `out/build/onion-arm/launcher/sprout-onion-check` without adding a device-behavior claim |

The Windows run must report all tests passed, including `mvp-host-integration`. The ARM command must complete successfully. A skipped, missing, flaky, or unexpectedly shortened gate is a failure until explained and rerun.

Automated tests create temporary synthetic fixtures and remove them on success or failure. Build products remain under ignored `out/`; they are not release evidence unless their commit and hashes are recorded.

## Desktop-manual gates

Use only ignored, sanitized preview data as described in [Getting Started](getting-started.md). Record the Sprout commit, command, screen checked, and outcome; screenshots must not contain household names or personal images.

- [ ] First launch shows the 640×480 setup flow; closing after a durable step and reopening resumes that step.
- [ ] Built-in parent and child portraits render; staged image crop remains controller-operable and stores only managed output.
- [ ] Parent and child profile modes are visibly distinct and navigable using keyboard and an SDL-compatible controller.
- [ ] Sanitized recent and favorite fixtures show the expected GB and SNES items; discovered child items remain unavailable without approval.
- [ ] Parent PIN entry is masked; unlock persists for the local day; manual lock returns to profile selection.
- [ ] Profile export/restore confirmations default safely, identify personal-image inclusion, and do not expose secrets or gameplay data.
- [ ] Repeated-start recovery offers only a validated last-known-good snapshot, defaults destructive confirmations to cancel, and visibly reports invalid state.

Delete or retain ignored preview data according to the test purpose. Never copy preview credentials, images, archives, or logs into Git.

## Hardware-only release gates

Follow the [Onion device check](onion-device-check.md) on a physically separate backed-up development card. Use one legally supplied GB test ROM and one legally supplied SNES test ROM. Record sanitized outcomes in the relevant issue; do not commit ROM names, saves, screenshots containing private data, or raw logs that identify household content.

- [ ] Device model, firmware revision, Onion version, Sprout commit, artifact SHA-256/size, ELF interpreter, and dynamic dependencies are recorded.
- [ ] The isolated ARM diagnostic exits successfully, returns to its invoking shell, and reports acceptable Argon2 duration and storage checks.
- [ ] GB and SNES requests reach Onion's fixed launch scripts with the ROM as one distinct argument; observed processes and exit status are recorded.
- [ ] Menu exit creates the expected save/state effects, updates Activity Tracker as observed, adds the game to GameSwitcher, and resumes it correctly.
- [ ] Normal emulator exit returns cleanly to Sprout without stale input, duplicate processes, or lost launcher state.
- [ ] Suspend/resume, power cycle, interrupted launch, and Onion's documented auto-resume escape are exercised without modifying the stable card.
- [ ] Child time is charged only while active across start, pause, resume, suspend, GameSwitcher, and return events; expiry completes Onion's normal save-and-exit path.
- [ ] Parent end-of-day unlock survives a normal reboot as intended, manual lock revokes it, and clock rollback fails closed.
- [ ] The reviewed physical safe-mode action bypasses Sprout and starts stock Onion without changing ROMs, saves, states, profiles, imports, exports, or backups.
- [ ] Three deliberately unfinished Sprout starts route to usable recovery; exit leaves failure evidence, restore/reset returns to ordinary UI, and another failure cannot trap the device in a recovery loop.

Issues #1, #5, #6, #7, #9, and #10 remain open until their hardware rows have direct evidence. A clean cross-build alone does not close them.

## Evidence record

For each run, retain a concise factual record outside the repository until sanitized for an issue:

```text
Sprout commit:
Device / firmware / Onion:
Artifact SHA-256 and size:
Exact command or action sequence:
Expected result:
Observed result and exit status:
Sanitized log or screenshot reference:
Cleanup performed:
Unexpected behavior / follow-up issue:
```

Preserve failed evidence before retrying. Do not edit a failed result into a pass, fabricate missing device facts, or test destructive startup behavior on a stable daily-use card.
