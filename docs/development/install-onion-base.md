# Install Sprout on a clean Onion card

Use this runbook for a second Miyoo SD card or after rebuilding a card. It
installs Sprout as an Onion integration; it never replaces the selected Onion
theme.

The startup sequence is firmware -> Onion initialization -> Sprout. The card
must then route straight to Sprout rather than showing Onion's MainUI carousel.

## Before starting

- Start from a card that boots cleanly into Onion and verify its model/Onion
  version is compatible with the target Miyoo.
- Back up the card or at least `Roms/`, `Saves/`, and `.tmp_update/config/`.
- Keep the card mounted at a known drive letter (examples below use `I:`).
- Do not copy a whole `.tmp_update` directory from another card. Preserve the
  target card's Wi-Fi, theme, and device settings.
- Fully power off the Miyoo before removing its SD card. Never remove the card
  while the device is on or showing a boot screen.

## Build the current package

From the repository root:

```powershell
rtk powershell -NoProfile -ExecutionPolicy Bypass -File tools/build-onion.ps1 -Action build
tar -czf out/onion-sprout-current.tar.gz -C out/package/onion-sprout .
```

The package is created at `out/package/onion-sprout/`. The build performs the
ARM artifact audit and package contract check.

Use the newly generated `out/onion-sprout-current.tar.gz`; do not substitute a
dated recovery archive.

## Direct-card installation (recommended for a fresh card)

With the target card mounted as `I:`:

```powershell
tar -xzf out/onion-sprout-current.tar.gz -C I:\
Copy-Item I:\App\Sprout\integration\runtime.sh I:\.tmp_update\runtime.sh -Force
Copy-Item I:\App\Sprout\integration\runtime.json I:\.tmp_update\config\sprout-runtime-integration.json -Force
```

The copy to `.tmp_update/runtime.sh` is the actual Onion routing hook. Merely
extracting `App/Sprout` leaves the card booting into stock Onion.

Install the curated library and its matching artwork only when desired:

```powershell
tar -xf out/household-payload-abridged.tar -C I:\
```

Keep the existing value in `I:\.tmp_update\config\active_theme`. Do not point
it at `Themes/Sprout`; Sprout does not require an Onion theme.

Verify before ejecting:

```powershell
chkdsk I:
Test-Path I:\App\Sprout\bin\sprout-launcher
Test-Path I:\App\Sprout\.containment-enabled
Get-FileHash I:\.tmp_update\runtime.sh,I:\App\Sprout\integration\runtime.sh
```

The two runtime hashes must match. Safely eject, then boot the Miyoo.

After boot, confirm that Sprout opens directly after the normal cold-boot
handoff, profiles and the dashboard render, and the selected base Onion theme
is unchanged.

## SSH installation (existing development card)

1. Enable Onion Wi-Fi and SSH.
2. Confirm the device's DHCP address (do not assume a previous address is
   still valid).
3. Authenticate as `root` using the device's Onion network password, supplied
   through a local secret or environment variable rather than a checked-in
   command.
4. Transfer `out/onion-sprout-current.tar.gz` to a temporary path on the card.
5. Validate the archive with `tar -tzf`, extract it at `/mnt/SDCARD`, then copy
   `App/Sprout/integration/runtime.sh` to `.tmp_update/runtime.sh` and its JSON
   beside the target card's configuration.
6. Restart only after checking the active theme still points to the target
   card's existing theme.

The device deployment service may be configured for either password or
public-key authentication. Use the configured key through `MIYOO_SSH_KEY` when
available; it is both faster and avoids placing a password in a command. The
one-command UI update and post-reboot hash check are documented in
[Miyoo live deployment](miyoo-live-deployment.md).

## Curated library and artwork

The base-Onion installation deliberately excludes a full ROM library. To add
the household's abridged, reviewed set and its matching `Imgs` artwork, extract
`out/household-payload-abridged.tar` at the card root. This installs the ROMs,
platform directories, catalogue metadata, and artwork together.

Do not copy an arbitrary full ROM tree when validating a new Sprout card.

## Recovery

### Parent escape to Onion

From any Sprout page while a **parent profile** is active, hold **Start +
Select** together for about 1.2 seconds. Sprout writes its authorized-exit
marker, releases the Onion framebuffer owner, and returns directly to Onion's
MainUI. The gesture is intentionally unavailable to child profiles; a normal
short Start press still opens Sprout's game focus page and Select still returns
to profile selection.

Hardware validation is required on both SDL input paths: keyboard
`Return`/`Escape` and controller `START`/`BACK`. The physical Miyoo may use
either mapping depending on its firmware/input configuration.

If the card cannot boot after an installation, do not remove it while the
Miyoo is running. Fully power off first, then mount the card on a computer.

To return to stock Onion without deleting ROMs or saves:

1. Remove `App/Sprout/.containment-enabled`.
2. Restore the target card's previous `.tmp_update/runtime.sh`.
3. Remove any queued `.tmp_update/cmd_to_run.sh` file.
4. Leave `.tmp_update/config/active_theme` set to a known-good Onion theme.

If the card becomes unreadable, first copy any recoverable files to a separate
disk before running repair or reformatting. Rebuild from a known-good Onion
base, then reinstall the newly built Sprout package and curated payload.
