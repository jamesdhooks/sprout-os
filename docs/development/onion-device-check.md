# Onion Device Check

This protocol validates Sprout's pinned ARM diagnostic on a dedicated development card. It does not install Sprout as the startup launcher, execute ROMs, alter Onion launch state, or establish GameSwitcher compatibility.

## Safety boundary

- Use a physically separate development card with Onion `v4.3.1-1` and a current backup.
- Do not run the diagnostic against the stable card or a directory containing household data.
- Use legally supplied test ROMs for later launch validation. Sprout does not provide ROMs or BIOS files.
- Keep Sprout files in a new isolated directory. The diagnostic refuses a non-empty data directory and never deletes it.
- Do not change startup, `cmd_to_run.sh`, `.runGameSwitcher`, saves, or activity data during this check.

## Build and audit the artifact

Docker must be running and network access is required when the pinned image, CMake archive, or source dependencies are not cached.

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\build-onion.ps1 -Action build
```

The output is `out/build/onion-arm/launcher/sprout-onion-check`. Before copying it, record its SHA-256, size, ELF interpreter, and dynamic dependencies. The expected architecture is 32-bit ARM EABI5 with `/lib/ld-linux-armhf.so.3`; `libstdc++` and `libgcc` should not be dynamic dependencies.

## Record the target baseline

On the powered development device, record:

```sh
cat /mnt/SDCARD/.tmp_update/onionVersion/version.txt
```

Also record the device model and firmware revision from the device's supported system-information surface. Do not infer or fabricate a firmware file path. Stop if the Onion version is not `v4.3.1-1` and reconcile the baseline before continuing.

## Run the isolated diagnostic

Copy the binary to a new development-only directory on the card and make it executable. From an SSH or serial shell, choose a new data directory and run:

```sh
chmod +x /mnt/SDCARD/sprout-dev/sprout-onion-check
/mnt/SDCARD/sprout-dev/sprout-onion-check \
  --data-dir /mnt/SDCARD/sprout-dev/check-data
```

A successful run reports the Onion baseline, pinned compiler image, profile/configuration/parent-access checks, Argon2 PIN setup time, and a typed launch-request identifier. Record the complete output and exit status. A crash, missing library, permission error, or unreasonable credential delay is a failed check; preserve the evidence before changing anything.

To additionally exercise read-only filename discovery against the development card, use:

```sh
/mnt/SDCARD/sprout-dev/sprout-onion-check \
  --data-dir /mnt/SDCARD/sprout-dev/check-data-with-library \
  --sd-root /mnt/SDCARD
```

This scans directory entries under `Roms/GB` and `Roms/SFC`. It does not open ROM content, inspect archives, or launch a game.

## Evidence to retain

Record these facts in the relevant issue without committing private logs or ROM names:

- Sprout commit and artifact SHA-256;
- device model, firmware revision, and Onion version;
- exact invocation and exit status;
- all diagnostic status lines and Argon2 duration;
- discovered-item count and sanitized warnings, if scanning was enabled;
- whether the device returned cleanly to the invoking shell;
- any missing dependencies, crashes, filesystem errors, or performance concerns.

## Separate launch and return validation

Issue-level hardware acceptance still requires one legal GB game and one legal SNES game to traverse Onion's normal launch contract. For each system, record the generated launch request, Onion runtime log when logging is deliberately enabled, menu-button save/exit, GameSwitcher appearance and resume, normal return to Sprout, and Activity Tracker result. Also test suspend, restart, interrupted launch, and Onion's documented auto-resume escape.

Those tests may alter saves and play activity and therefore belong on the development card only. Do not close the Onion adapter or hardware-investigation issues based on the command-line diagnostic alone.
