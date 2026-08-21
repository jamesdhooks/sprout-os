#!/bin/sh
# Restores the small Onion-style RetroArch launch wrappers required by the
# household seed systems. The device already supplies RetroArch and these
# pinned cores; this script intentionally installs no ROMs or emulator binary.
set -eu

SD_ROOT="${1:-/mnt/SDCARD}"
EMU_ROOT="$SD_ROOT/Emu"
RA_ROOT="$SD_ROOT/RetroArch"
TEMPLATE="$EMU_ROOT/GBA/launch.sh"

[ -x "$TEMPLATE" ]
[ -x "$RA_ROOT/retroarch" ]

install_wrapper() {
  system="$1"
  core="$2"
  target="$EMU_ROOT/$system"
  [ -f "$RA_ROOT/.retroarch/cores/$core" ]
  mkdir -p "$target"
  cp "$TEMPLATE" "$target/launch.sh"
  sed -i "s/gpsp_libretro.so/$core/" "$target/launch.sh"
  cp "$EMU_ROOT/GBA/cpufreq.sh" "$target/cpufreq.sh"
  chmod 755 "$target/launch.sh" "$target/cpufreq.sh"
}

install_wrapper FC fceumm_libretro.so
install_wrapper GBC gambatte_libretro.so
install_wrapper MD genesis_plus_gx_libretro.so
install_wrapper NEOGEO fbalpha2012_neogeo_libretro.so
install_wrapper PCE mednafen_pce_fast_libretro.so
install_wrapper PS pcsx_rearmed_libretro.so
install_wrapper SEGACD genesis_plus_gx_libretro.so
