# PICO-8 on Onion

Status: **Mouse & Cheese has a host-validated cartridge and deployment path; device launch remains unverified**.

Onion 4.3 documents fake-08 under `Roms/PICO`, accepting `.p8` and `.png`
cartridges. It supports subdirectories for multi-cart games, with the first
cart launched as the entry point. Compatibility is intentionally not assumed
to be complete. [Onion's fake-08 documentation](https://onionui.github.io/docs/emulators/pico-8)
is the source for these facts.

Onion also offers an Expert package for a PICO-8 native wrapper. It requires a
separately purchased PICO-8 Raspberry Pi package and locally supplied
`pico8_dyn` and `pico8.dat` files. Its behavior differs from fake-08: it can
use Splore and has greater cartridge compatibility, while fake-08 retains the
usual RetroArch facilities. [Onion's standalone documentation](https://onionui.github.io/docs/emulators/pico-8-standalone)
describes the requirements and controls.

## Integration boundary

- Treat `PICO` as a discoverable platform only when an extracted Onion card is present.
- Use fake-08 as the supported release backend; keep the native wrapper separate.
- Do not include licensed PICO-8 executables, `pico8.dat`, BBS downloads, or
  carts in this repository.
- Validate the exact `Emu/PICO` launcher, GameSwitcher return behavior, saves,
  and multi-cart handling on the target device before enabling launch in a
  child profile.
# PICO-8 integration

Sprout's first PICO-8 title is Mouse & Cheese. It is an original single-file `.p8` cartridge located in `pico8/mouse-cheese/`. The public cart is installed below `Roms/PICO/Sprout/MouseCheese/` and uses Onion's fake-08 launcher.

The supported file types and runtime boundaries remain those documented by Onion: fake-08 loads `.p8` and `.png` cartridges from `Roms/PICO`; the optional native PICO-8 wrapper requires user-supplied purchased files and is not distributed by Sprout. Mouse & Cheese deliberately avoids multi-cart loading.

Sprout's profile bridge produces a managed private cart copy whose only change is its `cartdata()` identifier. The managed directory is excluded from ordinary discovery, preventing duplicate library entries while retaining per-profile PICO save slots. Device behavior remains a hardware-validation item.
