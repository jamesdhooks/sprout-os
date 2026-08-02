# Onion Launch Adapter

Status: **desktop contract implemented; target execution unverified**.

The adapter is the only boundary permitted to translate an allowed emulated library item into an Onion process launch. Version 0.1 supports the pinned Onion `v4.3.1-1` Game Boy and SNES system packages only.

## Input contract

An emulated launch target contains:

- a non-empty library item identity;
- the typed Onion system (`GB` or `SFC`);
- an absolute local ROM path; and
- the caller's explicit profile-policy decision.

The adapter does not accept a shell command, core path, environment override, or caller-selected executable.

## Validation and execution

Before starting a process, the adapter:

1. rejects a denied policy decision;
2. resolves the SD-card root, system ROM root, ROM, and launcher canonically;
3. requires the ROM and launcher to remain inside the configured SD-card boundary;
4. requires a regular ROM file with an extension allowed by the pinned Onion package; and
5. invokes the fixed system `launch.sh` with the ROM path as one distinct argument.

| System | ROM root | Launcher | Allowed extensions |
| --- | --- | --- | --- |
| Game Boy | `/mnt/SDCARD/Roms/GB` | `/mnt/SDCARD/Emu/GB/launch.sh` | `bin`, `dmg`, `gb`, `gbc`, `zip`, `7z` |
| SNES | `/mnt/SDCARD/Roms/SFC` | `/mnt/SDCARD/Emu/SFC/launch.sh` | `sfc`, `smc`, `fig`, `bs`, `st`, `zip`, `7z` |

Onion's package script remains responsible for its configured RetroArch core and environment. Sprout does not reproduce or override that behavior.

## Results

Every attempt produces one structured outcome: completed, policy denied, invalid target, missing ROM, unsupported ROM, launcher unavailable, process start failed, or abnormal exit. An abnormal exit retains an exit code when the operating system supplies one. Raw filesystem or process errors are not exposed as navigation targets.

## Verification boundary

Desktop contract tests cover GB/SNES mapping, paths containing spaces, denied and malformed targets, missing files, unsupported extensions, missing launchers, start failure, non-zero exit, and missing exit status. Windows intentionally cannot execute Onion shell scripts.

The following remain open until a development card is available:

- toolchain compatibility of the POSIX process runner;
- installed paths, permissions, environment, and exit behavior;
- launch of one legally supplied GB and SNES ROM;
- activity, save, state, GameSwitcher, and return effects; and
- recovery from an interrupted or failed launch.

These hardware findings belong in [Onion integration research](../research/onion-integration.md), not in desktop test claims.
