#include "sprout/launcher/onion_launch_adapter.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <system_error>
#include <utility>

namespace sprout::launcher {
namespace {

std::string lowercase(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

bool within(const std::filesystem::path& path,
            const std::filesystem::path& root) {
  auto path_part = path.begin();
  for (auto root_part = root.begin(); root_part != root.end();
       ++root_part, ++path_part) {
    if (path_part == path.end() || *path_part != *root_part) {
      return false;
    }
  }
  return true;
}

LaunchResult result(const EmulatedLaunchTarget& target, LaunchOutcome outcome,
                    std::string detail,
                    std::optional<int> exit_code = std::nullopt) {
  return LaunchResult{
      .outcome = outcome,
      .item_id = target.item_id,
      .exit_code = exit_code,
      .detail = std::move(detail),
  };
}

}  // namespace

std::optional<OnionSystemContract> onion_system_contract(OnionSystem system) {
  static constexpr std::array<std::string_view, 6> game_boy_extensions{
      ".bin", ".dmg", ".gb", ".gbc", ".zip", ".7z"};
  static constexpr std::array<std::string_view, 6> game_boy_color_extensions{
      ".bin", ".dmg", ".gb", ".gbc", ".zip", ".7z"};
  static constexpr std::array<std::string_view, 4> game_boy_advance_extensions{
      ".bin", ".gba", ".zip", ".7z"};
  static constexpr std::array<std::string_view, 6> nes_extensions{
      ".fds", ".nes", ".unif", ".unf", ".zip", ".7z"};
  static constexpr std::array<std::string_view, 7> snes_extensions{
      ".sfc", ".smc", ".fig", ".bs", ".st", ".zip", ".7z"};
  static constexpr std::array<std::string_view, 11> genesis_extensions{
      ".gen", ".smd", ".md", ".32x", ".bin", ".iso", ".sms",
      ".68k", ".chd", ".zip", ".7z"};
  static constexpr std::array<std::string_view, 11> master_system_extensions{
      ".gen", ".smd", ".md", ".32x", ".bin", ".iso", ".sms",
      ".68k", ".chd", ".zip", ".7z"};
  static constexpr std::array<std::string_view, 4> game_gear_extensions{
      ".bin", ".gg", ".zip", ".7z"};
  static constexpr std::array<std::string_view, 12> sega_cd_extensions{
      ".gen", ".smd", ".md", ".32x", ".cue", ".iso", ".sms",
      ".68k", ".chd", ".m3u", ".zip", ".7z"};
  static constexpr std::array<std::string_view, 8> turbografx_extensions{
      ".pce", ".ccd", ".iso", ".img", ".chd", ".cue", ".zip", ".7z"};
  static constexpr std::array<std::string_view, 2> neo_geo_extensions{
      ".zip", ".7z"};
  static constexpr std::array<std::string_view, 1> arcade_extensions{".zip"};
  static constexpr std::array<std::string_view, 10> playstation_extensions{
      ".iso", ".cue", ".img", ".mdf", ".pbp", ".toc", ".cbn",
      ".m3u", ".ccd", ".chd"};
  static constexpr std::array<std::string_view, 2> pico8_extensions{".p8", ".png"};
  switch (system) {
    case OnionSystem::GameBoy:
      return OnionSystemContract{
          .id = "GB",
          .rom_directory = "Roms/GB",
          .launcher = "Emu/GB/launch.sh",
          .extensions = game_boy_extensions,
      };
    case OnionSystem::GameBoyColor:
      return OnionSystemContract{
          .id = "GBC",
          .rom_directory = "Roms/GBC",
          .launcher = "Emu/GBC/launch.sh",
          .extensions = game_boy_color_extensions,
      };
    case OnionSystem::GameBoyAdvance:
      return OnionSystemContract{
          .id = "GBA",
          .rom_directory = "Roms/GBA",
          .launcher = "Emu/GBA/launch.sh",
          .extensions = game_boy_advance_extensions,
      };
    case OnionSystem::NintendoEntertainmentSystem:
      return OnionSystemContract{
          .id = "NES",
          .rom_directory = "Roms/FC",
          .launcher = "Emu/FC/launch.sh",
          .extensions = nes_extensions,
      };
    case OnionSystem::SuperNintendo:
      return OnionSystemContract{
          .id = "SFC",
          .rom_directory = "Roms/SFC",
          .launcher = "Emu/SFC/launch.sh",
          .extensions = snes_extensions,
      };
    case OnionSystem::SegaGenesis:
      return OnionSystemContract{
          .id = "GEN",
          .rom_directory = "Roms/MD",
          .launcher = "Emu/MD/launch.sh",
          .extensions = genesis_extensions,
      };
    case OnionSystem::SegaMasterSystem:
      return OnionSystemContract{
          .id = "SMS",
          .rom_directory = "Roms/MS",
          .launcher = "Emu/MS/launch.sh",
          .extensions = master_system_extensions,
      };
    case OnionSystem::SegaGameGear:
      return OnionSystemContract{
          .id = "GG",
          .rom_directory = "Roms/GG",
          .launcher = "Emu/GG/launch.sh",
          .extensions = game_gear_extensions,
      };
    case OnionSystem::SegaCD:
      return OnionSystemContract{
          .id = "SCD",
          .rom_directory = "Roms/SEGACD",
          .launcher = "Emu/SEGACD/launch.sh",
          .extensions = sega_cd_extensions,
      };
    case OnionSystem::TurboGrafx16:
      return OnionSystemContract{
          .id = "PCE",
          .rom_directory = "Roms/PCE",
          .launcher = "Emu/PCE/launch.sh",
          .extensions = turbografx_extensions,
      };
    case OnionSystem::NeoGeo:
      return OnionSystemContract{
          .id = "NEOGEO",
          .rom_directory = "Roms/NEOGEO",
          .launcher = "Emu/NEOGEO/launch.sh",
          .extensions = neo_geo_extensions,
      };
    case OnionSystem::Arcade:
      return OnionSystemContract{
          .id = "ARCADE",
          .rom_directory = "Roms/ARCADE",
          .launcher = "Emu/ARCADE/launch.sh",
          .extensions = arcade_extensions,
      };
    case OnionSystem::PlayStation:
      return OnionSystemContract{
          .id = "PS",
          .rom_directory = "Roms/PS",
          .launcher = "Emu/PSX/launch.sh",
          .extensions = playstation_extensions,
      };
    case OnionSystem::Pico8:
      return OnionSystemContract{
          .id = "PICO",
          .rom_directory = "Roms/PICO",
          .launcher = "Emu/PICO/launch.sh",
          .extensions = pico8_extensions,
      };
  }
  return std::nullopt;
}

ReadOnlyView<OnionSystem> onion_supported_systems() {
  static constexpr std::array systems{
      OnionSystem::GameBoy,
      OnionSystem::GameBoyColor,
      OnionSystem::GameBoyAdvance,
      OnionSystem::NintendoEntertainmentSystem,
      OnionSystem::SuperNintendo,
      OnionSystem::SegaGenesis,
      OnionSystem::SegaMasterSystem,
      OnionSystem::SegaGameGear,
      OnionSystem::SegaCD,
      OnionSystem::TurboGrafx16,
      OnionSystem::NeoGeo,
      OnionSystem::Arcade,
      OnionSystem::PlayStation,
      OnionSystem::Pico8,
  };
  return systems;
}

OnionLaunchAdapter::OnionLaunchAdapter(std::filesystem::path sd_card_root,
                                       LaunchProcess& process)
    : sd_card_root_(std::move(sd_card_root)), process_(process) {}

LaunchResult OnionLaunchAdapter::launch(
    const EmulatedLaunchTarget& target) const {
  if (!target.launch_allowed) {
    return result(target, LaunchOutcome::PolicyDenied,
                  "The active profile does not allow this item");
  }
  if (target.item_id.empty() || target.rom_path.empty() ||
      !target.rom_path.is_absolute()) {
    return result(target, LaunchOutcome::InvalidTarget,
                  "The library item does not contain a valid local ROM target");
  }

  const auto contract = onion_system_contract(target.system);
  if (!contract.has_value()) {
    return result(target, LaunchOutcome::InvalidTarget,
                  "The Onion system is not supported by this adapter version");
  }

  std::error_code error;
  const auto sd_card_root =
      std::filesystem::weakly_canonical(sd_card_root_, error);
  if (error || !std::filesystem::is_directory(sd_card_root, error) || error) {
    return result(target, LaunchOutcome::InvalidTarget,
                  "The configured Onion SD-card root is unavailable");
  }
  const auto rom_root =
      std::filesystem::weakly_canonical(sd_card_root / contract->rom_directory,
                                        error);
  if (error || !within(rom_root, sd_card_root)) {
    return result(target, LaunchOutcome::InvalidTarget,
                  "The configured Onion ROM root cannot be resolved");
  }
  const auto rom_path = std::filesystem::weakly_canonical(target.rom_path, error);
  if (error || !within(rom_path, rom_root)) {
    return result(target, LaunchOutcome::InvalidTarget,
                  "The ROM target is outside its configured Onion system directory");
  }
  if (!std::filesystem::is_regular_file(rom_path, error) || error) {
    return result(target, LaunchOutcome::MissingRom,
                  "The selected ROM file is unavailable");
  }

  const auto extension = lowercase(rom_path.extension().string());
  if (std::find(contract->extensions.begin(), contract->extensions.end(),
                extension) == contract->extensions.end()) {
    return result(target, LaunchOutcome::UnsupportedRom,
                  "The ROM extension is not supported by the pinned Onion system");
  }

  const auto launcher =
      std::filesystem::weakly_canonical(sd_card_root / contract->launcher, error);
  if (error || !within(launcher, sd_card_root) ||
      !std::filesystem::is_regular_file(launcher, error) || error) {
    return result(target, LaunchOutcome::LauncherUnavailable,
                  "The pinned Onion system launcher is unavailable");
  }

  const auto process_result = process_.run(launcher, {rom_path.string()});
  if (!process_result.started) {
    return result(target, LaunchOutcome::ProcessStartFailed,
                  process_result.detail.empty() ? "The Onion launcher could not be started"
                                                : process_result.detail);
  }
  if (!process_result.exit_code.has_value() || *process_result.exit_code != 0) {
    return result(target, LaunchOutcome::AbnormalExit,
                  process_result.detail.empty() ? "The Onion launcher exited abnormally"
                                                : process_result.detail,
                  process_result.exit_code);
  }
  return result(target, LaunchOutcome::Completed, "The game returned normally", 0);
}

}  // namespace sprout::launcher
