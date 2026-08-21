#pragma once

#include "sprout/launcher/launch_process.hpp"

#include <filesystem>

namespace sprout::launcher {

inline constexpr int kOnionHandoffExitCode = 75;
// Reserved for a user-requested short power press. The wrapper owns suspend so
// it can release Sprout's framebuffer lock before the kernel sleeps.
inline constexpr int kOnionSleepExitCode = 74;

class OnionRuntimeHandoffProcess final : public LaunchProcess {
 public:
  explicit OnionRuntimeHandoffProcess(std::filesystem::path runtime_root);

  [[nodiscard]] ProcessResult run(
      const std::filesystem::path& executable,
      const std::vector<std::string>& arguments) override;

 private:
  std::filesystem::path runtime_root_;
};

}  // namespace sprout::launcher
