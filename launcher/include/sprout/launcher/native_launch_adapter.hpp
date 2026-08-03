#pragma once

#include "sprout/launcher/launch_process.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace sprout::launcher {

struct NativeLaunchTarget {
  std::string item_id;
  std::filesystem::path package_root;
  std::string profile_id;
  std::uint64_t seed{1};
  bool launch_allowed{false};
};

enum class NativeLaunchOutcome {
  Completed,
  PolicyDenied,
  InvalidTarget,
  PackageUnavailable,
  RuntimeUnavailable,
  ProcessStartFailed,
  AbnormalExit,
};

enum class NativeLaunchMode {
  Interactive,
  SmokeTest,
};

struct NativeLaunchResult {
  NativeLaunchOutcome outcome;
  std::string item_id;
  std::optional<int> exit_code;
  std::string detail;

  [[nodiscard]] bool completed() const noexcept {
    return outcome == NativeLaunchOutcome::Completed;
  }
};

class NativeLaunchAdapter {
 public:
  NativeLaunchAdapter(std::filesystem::path runtime_executable,
                      std::filesystem::path storage_root,
                      LaunchProcess& process);

  [[nodiscard]] NativeLaunchResult launch(
      const NativeLaunchTarget& target,
      NativeLaunchMode mode = NativeLaunchMode::Interactive) const;

 private:
  std::filesystem::path runtime_executable_;
  std::filesystem::path storage_root_;
  LaunchProcess& process_;
};

}  // namespace sprout::launcher
