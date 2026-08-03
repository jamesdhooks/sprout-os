#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>

namespace sprout::launcher {

inline constexpr std::uint32_t kStartupHealthSchemaVersion = 1;
inline constexpr std::uint32_t kStartupRecoveryFailureThreshold = 3;

struct StartupDecision {
  std::uint64_t attempt_id;
  std::uint32_t consecutive_failures;
  bool recovery_required;
};

class StartupHealthStore {
 public:
  explicit StartupHealthStore(std::filesystem::path database_path);
  ~StartupHealthStore();

  StartupHealthStore(const StartupHealthStore&) = delete;
  StartupHealthStore& operator=(const StartupHealthStore&) = delete;
  StartupHealthStore(StartupHealthStore&&) noexcept;
  StartupHealthStore& operator=(StartupHealthStore&&) noexcept;

  [[nodiscard]] std::uint32_t database_schema_version() const;
  [[nodiscard]] StartupDecision begin_startup();
  void mark_ready(std::uint64_t attempt_id);

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace sprout::launcher
