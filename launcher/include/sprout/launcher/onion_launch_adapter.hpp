#pragma once

#include <filesystem>
#include <optional>
#include "sprout/launcher/read_only_view.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace sprout::launcher {

enum class OnionSystem {
  GameBoy,
  SuperNintendo,
};

struct OnionSystemContract {
  std::string_view id;
  std::filesystem::path rom_directory;
  std::filesystem::path launcher;
  ReadOnlyView<std::string_view> extensions;
};

[[nodiscard]] std::optional<OnionSystemContract> onion_system_contract(
    OnionSystem system);

struct EmulatedLaunchTarget {
  std::string item_id;
  OnionSystem system;
  std::filesystem::path rom_path;
  bool launch_allowed{false};
};

struct ProcessResult {
  bool started{false};
  std::optional<int> exit_code;
  std::string detail;
};

class LaunchProcess {
 public:
  virtual ~LaunchProcess() = default;
  [[nodiscard]] virtual ProcessResult run(
      const std::filesystem::path& executable,
      const std::vector<std::string>& arguments) = 0;
};

class OnionLaunchProcess final : public LaunchProcess {
 public:
  [[nodiscard]] ProcessResult run(
      const std::filesystem::path& executable,
      const std::vector<std::string>& arguments) override;
};

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
