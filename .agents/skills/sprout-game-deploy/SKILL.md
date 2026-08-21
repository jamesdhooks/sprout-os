---
name: sprout-game-deploy
description: Run, preview, package, or deploy Sprout native games and PICO-8 carts to Windows, an Onion SD card, or a Miyoo device over SSH. Use for desktop launch, mounted-card installation, Sprout catalogue updates, profile-scoped PICO carts, fake-08, the licensed PICO-8 wrapper, SSH transfer, GameSwitcher return, or device acceptance.
---

# Sprout game deployment

Validate locally, deploy only Sprout-owned paths, and report platform evidence precisely.

## Select the target

Read references/deployment-targets.md and the relevant development guide.

- Windows native preview: build and run the Sprout launcher/runtime package.
- Desktop PICO-8: use the licensed local executable through tools/pico8_game.py run.
- Mounted Onion card: use the repository deploy tools with an explicit SD root.
- SSH Miyoo: use tools/pico8_game.py deploy-ssh for one PICO cart or the documented native package flow.

## Validate before transfer

- Confirm the exact package/cart and manifest.
- Run builders, validators, campaign checks, and required captures.
- Resolve and inspect the explicit target path.
- Preserve unrelated catalogue entries, ROMs, saves, wrapper files, and profile data.
- Use dry-run where available.

## PICO-8 commands

~~~powershell
python tools/pico8_game.py deploy <slug> --sd-root G:\
python tools/pico8_game.py deploy <slug> --sd-root G:\ --profile-id son
python tools/pico8_game.py deploy-ssh <slug> --host onion@<ip> --dry-run
python tools/pico8_game.py deploy-ssh <slug> --host onion@<ip>
powershell -ExecutionPolicy Bypass -File tools/pico8-deploy.ps1 -SdRoot G:\
~~~

Use the all-cart PowerShell deployer for a release because it performs shared-code and campaign checks. Profile deployment may alter only the designated legal cartdata() ID in the managed copy.

## Licensed runtime boundary

Never commit or redistribute pico8.exe, pico8_dyn, pico8.dat, private saves, purchased downloads, or BBS carts. The user supplies licensed runtime files to Onion's expected wrapper location. Keep fake-08 as the required compatibility target even when the licensed wrapper is installed.

## Device acceptance

Verify on physical hardware:

- visible title and cover in the expected system;
- controls, held input, and pause;
- frame pacing and audio;
- public and profile-scoped persistence;
- back/exit and GameSwitcher return;
- relaunch, reinstall, and upgrade behavior;
- representative early/middle/late content.

Record Onion version, device model, runtime, cart/package version, commands, and remaining unverified behavior. A running process alone is not a visual, input, audio, or return-path pass.
