# Launcher MVP Hardware Halt

Status: **Blocked on development-card and target-device evidence**

The Sprout Family Launcher MVP reached its last host-verifiable checkpoint at commit `5097eb7e254ae145f0932508c2faba5e0a806e47` on 2026-08-02. Work after that commit began the independent Sprout Arcade Windows track; it does not imply that the launcher MVP's Onion hardware gates were completed.

## Completed before the halt

The checkpoint contains the project foundation, Windows launcher preview, resumable setup, profiles, parent access, profile archive/restore, daily-time accounting core, local GB/SNES discovery, typed Onion launch boundary, repeated-start recovery, host integration evidence, and a pinned Onion ARM diagnostic build with artifact auditing.

Host and cross-build evidence is recorded in the [MVP evidence checklist](mvp-evidence.md). It proves portable state and decision boundaries, typed requests, Windows presentation behavior, and ARM build compatibility. It does not prove execution on a Miyoo device.

## Blocking evidence

The remaining milestone work requires a dedicated, backed-up Onion development SD card and target device. The available removable media was not an Onion card and was deliberately left untouched.

The following issues remain open:

| Issue | Required device evidence |
| --- | --- |
| [#1 Pin and validate the Onion baseline](https://github.com/jamesdhooks/sprout-os/issues/1) | Confirm the pinned Onion version and reviewed paths on the development card |
| [#5 Implement the Onion launch adapter](https://github.com/jamesdhooks/sprout-os/issues/5) | Execute the GB/SNES launch contract on target |
| [#6 Preserve GameSwitcher and Sprout return](https://github.com/jamesdhooks/sprout-os/issues/6) | Verify launch, suspend, resume, exit, and return lifecycle |
| [#7 Implement local child daily-time policy](https://github.com/jamesdhooks/sprout-os/issues/7) | Connect and verify real target lifecycle enforcement |
| [#9 Implement stock-Onion safe-mode recovery](https://github.com/jamesdhooks/sprout-os/issues/9) | Select and prove a physical boot bypass into stock Onion |
| [#10 Add launcher MVP integration and hardware-test coverage](https://github.com/jamesdhooks/sprout-os/issues/10) | Complete the hardware-only evidence matrix |

## Resume point

When the dedicated card and device are available:

1. Start from the tagged pre-Arcade checkpoint and the [Onion device check](onion-device-check.md).
2. Positively identify the card and back it up before any write.
3. Run the evidence sequence in [MVP evidence](mvp-evidence.md).
4. Record exact Onion version, paths, commands, artifacts, and observed lifecycle behavior.
5. Keep unverified behavior behind existing adapters and leave failed hardware gates open.

Sprout Arcade may progress on Windows while this halt remains in effect. Arcade runtime, game, and asset work must not modify or close these device issues without direct evidence.
