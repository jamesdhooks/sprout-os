#pragma once

#include "sprout/launcher/launch_process.hpp"

#include <filesystem>
#include <optional>
#include "sprout/launcher/read_only_view.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace sprout::launcher {

enum class OnionSystem {
  GameBoy,
  GameBoyColor,
  GameBoyAdvance,
  NintendoEntertainmentSystem,
  SuperNintendo,
  SegaGenesis,
  SegaMasterSystem,
  SegaGameGear,
  SegaCD,
  TurboGrafx16,
  NeoGeo,
  Arcade,
  PlayStation,
  Pico8,
};

struct OnionSystemContract {
  std::string_view id;
  std::filesystem::path rom_directory;
  std::filesystem::path launcher;
  ReadOnlyView<std::string_view> extensions;
};

[[nodiscard]] std::optional<OnionSystemContract> onion_system_contract(
    OnionSystem system);
[[nodiscard]] ReadOnlyView<OnionSystem> onion_supported_systems();

struct EmulatedLaunchTarget {
  std::string item_id;
  OnionSystem system;
  std::filesystem::path rom_path;
  bool launch_allowed{false};
};

using OnionLaunchProcess = SystemLaunchProcess;

enum class LaunchOutcome {
  Completed,
  PolicyDenied,
  InvalidTarget,
  MissingRom,
  UnsupportedRom,
  LauncherUnavailable,
  ProcessStartFailed,
  AbnormalExit,
};

struct LaunchResult {
  LaunchOutcome outcome;
  std::string item_id;
  std::optional<int> exit_code;
  std::string detail;

  [[nodiscard]] bool completed() const noexcept {
    return outcome == LaunchOutcome::Completed;
  }
};

class OnionLaunchAdapter {
 public:
  OnionLaunchAdapter(std::filesystem::path sd_card_root,
                     LaunchProcess& process);

  [[nodiscard]] LaunchResult launch(const EmulatedLaunchTarget& target) const;

 private:
  std::filesystem::path sd_card_root_;
  LaunchProcess& process_;
};

}  // namespace sprout::launcher
