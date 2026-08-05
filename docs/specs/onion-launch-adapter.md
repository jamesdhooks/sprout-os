# Onion Launch Adapter

Status: **desktop contract implemented; target execution unverified**.

The adapter is the only boundary permitted to translate an allowed emulated library item into an Onion process launch. It uses a platform registry so discovery, profile curation, and launch validation share one explicit Onion contract.

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
| Seed label | Onion ROM path | Onion launcher path | Extensions |
| --- | --- | --- | --- |
| `GB` | `Roms/GB` | `Emu/GB/launch.sh` | `bin`, `dmg`, `gb`, `gbc`, `zip`, `7z` |
| `GBC` | `Roms/GBC` | `Emu/GBC/launch.sh` | `bin`, `dmg`, `gb`, `gbc`, `zip`, `7z` |
| `GBA` | `Roms/GBA` | `Emu/GBA/launch.sh` | `bin`, `gba`, `zip`, `7z` |
| `NES` | `Roms/FC` | `Emu/FC/launch.sh` | `fds`, `nes`, `unif`, `unf`, `zip`, `7z` |
| `SFC` | `Roms/SFC` | `Emu/SFC/launch.sh` | `sfc`, `smc`, `fig`, `bs`, `st`, `zip`, `7z` |
| `GEN` | `Roms/MD` | `Emu/MD/launch.sh` | Onion PicoDrive Genesis set |
| `SMS` | `Roms/MS` | `Emu/MS/launch.sh` | Onion PicoDrive Master System set |
| `GG` | `Roms/GG` | `Emu/GG/launch.sh` | `bin`, `gg`, `zip`, `7z` |
| `SCD` | `Roms/SEGACD` | `Emu/SEGACD/launch.sh` | Onion PicoDrive Sega CD set |
| `PCE` | `Roms/PCE` | `Emu/PCE/launch.sh` | `pce`, `ccd`, `iso`, `img`, `chd`, `cue`, `zip`, `7z` |
| `NEOGEO` | `Roms/NEOGEO` | `Emu/NEOGEO/launch.sh` | `zip`, `7z` |
| `ARCADE` | `Roms/ARCADE` | `Emu/ARCADE/launch.sh` | `zip` |
| `PS` | `Roms/PS` | `Emu/PSX/launch.sh` | Onion PCSX ReARMed image set |
| `PICO` | `Roms/PICO` | `Emu/PICO/launch.sh` | `p8`, `png` |

These paths and extensions come from Onion's upstream package definitions at
commit `07505ea58c7bba698d6b9220ff43946a43cac76b`, which matches the build
identifier on the attached card. Sprout scans an absent optional ROM directory
silently; it does not manufacture warnings or library entries for a system that
is not installed. The launch adapter still rejects an item when its resolved
launcher script is missing.

Onion's package script remains responsible for its configured RetroArch core and environment. Sprout does not reproduce or override that behavior.

## Results

Every attempt produces one structured outcome: completed, policy denied, invalid target, missing ROM, unsupported ROM, launcher unavailable, process start failed, or abnormal exit. An abnormal exit retains an exit code when the operating system supplies one. Raw filesystem or process errors are not exposed as navigation targets.

## Verification boundary

Desktop contract tests cover every listed platform, paths containing spaces, denied and malformed targets, missing files, unsupported extensions, missing launchers, start failure, non-zero exit, and missing exit status. Windows intentionally cannot execute Onion shell scripts. A Linux host test additionally executes the fixed script through the POSIX runner and covers normal return, a non-zero exit, and missing execute permission.

The following remain open until a development card is available:

- toolchain compatibility of the POSIX process runner;
- installed paths, permissions, environment, and exit behavior;
- launch and clean return of at least one legally supplied ROM for each enabled family;
- activity, save, state, GameSwitcher, and return effects; and
- recovery from an interrupted or failed launch.

These hardware findings belong in [Onion integration research](../research/onion-integration.md), not in desktop test claims.
