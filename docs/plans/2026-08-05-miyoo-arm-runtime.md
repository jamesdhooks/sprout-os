# Miyoo ARM Launcher and Native Runtime Implementation Plan

> **For Hermes:** Use subagent-driven-development skill to implement this plan task-by-task.

**Goal:** Build, package, deploy, and verify a rollback-safe Sprout launcher and one native Sprout game on Onion v4.3.1-1 / Miyoo Mini Plus.

**Architecture:** Cross-link the existing SDL2 launcher/runtime against an untracked SDK overlay harvested from the verified Onion device. Keep Sprout isolated as an Onion `App/Sprout` package, use private `LD_LIBRARY_PATH`, preserve the current Onion UI/startup path, and deploy over SSH with backup/hash verification. Reuse the current launcher CLI and native-game package format; do not replace startup or claim emulated-ROM launch compatibility in this slice.

**Tech Stack:** C++20, CMake/Ninja, pinned Miyoo ARM GCC 8.3 Docker toolchain, Onion SDL2/SDL2_image ABI, Lua 5.4, Python package tooling, SSH/tar deployment.

---

### Task 1: Lock the package contract in RED

**Objective:** Define the minimum deployable vertical slice before production changes.

**Files:**
- Create: `tools/onion_package_contract_test.py`

**Steps:**
1. Require `App/Sprout` launcher/runtime ELF files, private SDL libraries, `launch.sh`, `config.json`, household seed, Blocks & Buttons package, and a complete hash manifest.
2. Run `python tools/onion_package_contract_test.py`.
3. Expected RED: missing required package files.
4. Commit the test only with the first GREEN implementation task.

### Task 2: Add an Onion SDL2 SDK mode and ARM UI/runtime targets

**Objective:** Cross-build the existing launcher and runtime against Onion’s proven SDL2 ABI without modifying the renderer.

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `CMakePresets.json`
- Modify: `launcher/CMakeLists.txt`
- Modify: `runtime/CMakeLists.txt`
- Test: `tools/onion_package_contract_test.py`

**Steps:**
1. Add `SPROUT_BUILD_ONION_UI` and required `SPROUT_ONION_SDK_ROOT` cache options.
2. For Onion UI builds, create imported SDL2 and SDL2_image targets from `out/onion-sdk`; retain existing FetchContent targets for desktop.
3. Build `sprout-launcher` and `sprout-runtime` when desktop or Onion UI is enabled; omit SDL2main on Linux and add GCC 8 `stdc++fs` linkage where required.
4. Enable runtime/UI in the `onion-arm` preset.
5. Run the pinned Onion build; expected GREEN for both ARM executables.
6. Run all 35 desktop tests; expected unchanged pass.

### Task 3: Build the deterministic Onion App package

**Objective:** Produce a complete, hash-manifested `App/Sprout` package.

**Files:**
- Create: `tools/package_onion.py`
- Create: `tools/templates/onion-sprout-launch.sh`
- Create: `tools/templates/onion-sprout-config.json`
- Modify: `tools/build-onion.ps1`
- Test: `tools/onion_package_contract_test.py`

**Steps:**
1. Copy ARM launcher/runtime, private SDL libraries, launcher/runtime assets, Blocks & Buttons package, and the current household seed into `out/package/onion-sprout/App/Sprout`.
2. Write LF-only `launch.sh` that sets private library paths and passes `--data-dir`, `--sd-root`, `--arcade-root`, `--runtime`, and `--household-seed`.
3. Generate `deployment-manifest.json` with SHA-256 and byte size for every package file.
4. Run `python tools/onion_package_contract_test.py`; expected GREEN.
5. Run package script twice and compare manifest/package hashes; expected deterministic output.

### Task 4: Add remote headless smoke gates

**Objective:** Prove ARM launcher/runtime behavior over SSH before opening the display.

**Files:**
- Modify: package scripts/templates only if required by observed behavior.

**Steps:**
1. Deploy package into a versioned staging directory under `/mnt/SDCARD/sprout-dev/deployments` using tar-over-SSH.
2. Verify every device file against `deployment-manifest.json` before activation.
3. Run launcher `--arcade-smoke-test --arcade-smoke-item arcade:sprout.blocks-buttons` with the package paths.
4. Expected: runtime exit 0 and launcher reports completed return.
5. Preserve logs/data and pull them to `Q:/miyoo/card-a-device-evidence`.

### Task 5: Activate a rollback-safe Onion App entry

**Objective:** Make Sprout launchable from Onion Apps without changing Onion startup.

**Files:**
- Device target: `/mnt/SDCARD/App/Sprout`

**Steps:**
1. If an existing target exists, move it to `/mnt/SDCARD/sprout-dev/backups/<timestamp>/App-Sprout`.
2. Atomically rename the verified staging directory to `/mnt/SDCARD/App/Sprout`.
3. Re-hash the active package on-device.
4. Launch through SSH once with logging; stop on any crash or missing dependency.
5. Ask James for the visual/input acceptance test from Apps → Sprout.

### Task 6: Review and evidence

**Objective:** Prove the implementation meets scope and remains rollback-safe.

**Files:**
- Create/update: `Q:/miyoo/card-a-sprout-arm-deployment-report.json`

**Steps:**
1. Run spec-compliance review.
2. Run code-quality/security review.
3. Run full desktop suite, ARM build audit, package contract, and remote smoke tests.
4. Record source commit, artifact hashes, on-device hashes, Onion version, logs, backup path, and remaining boundaries.
5. Remove temporary password helper after deployment evidence is captured.
