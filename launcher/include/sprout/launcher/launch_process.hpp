#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace sprout::launcher {

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

class SystemLaunchProcess final : public LaunchProcess {
 public:
  [[nodiscard]] ProcessResult run(
      const std::filesystem::path& executable,
      const std::vector<std::string>& arguments) override;
};

}  // namespace sprout::launcher
