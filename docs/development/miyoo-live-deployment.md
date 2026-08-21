# Miyoo live deployment

Use this path for an already-installed Sprout card. It intentionally updates
only the launcher binary and UI icon assets; it does not replace Onion,
themes, saves, the household library, or the containment runtime.

## One-time connection setup

Set the device address and its private key in the current shell. Keep the key
outside the repository.

```powershell
$env:MIYOO_HOST = "192.168.2.72"
$env:MIYOO_SSH_KEY = "$env:USERPROFILE\.ssh\id_ed25519_miyoo_card_a"
```

Confirm access before building or changing device files:

```powershell
python tools\miyoo_live.py --check
```

## Fast UI iteration

From the repository root, run the ARM build, package validation, delta archive,
transfer, reboot, and post-boot hash verification as one deliberate sequence:

```powershell
powershell -ExecutionPolicy Bypass -File tools\build-onion.ps1
python tools\package_onion.py
python tools\onion_package_contract_test.py
python tools\miyoo_live.py --deploy-ui
```

`--deploy-ui` creates its archive from the freshly packaged output, uploads it
to a temporary card path, extracts only `App/Sprout/bin/sprout-launcher` and
`App/Sprout/bin/assets/icons`, then waits for the reboot and compares the live
launcher SHA-256 with the local package. A mismatch is a failure, not a
successful deployment.

For a code-only change, do not copy the full package, household seed, ROMs,
artwork, runtime wrapper, splash, or Onion theme. Those transfers are slower
and introduce unrelated boot risk.

## Recovery

If `--check` cannot connect, first confirm Wi-Fi and the address on the device.
If the device is reachable but the key is rejected, use the exact configured
key rather than falling back to a new password bootstrap. Do not remove the SD
card while the device is running.
