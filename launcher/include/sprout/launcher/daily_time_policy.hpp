#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace sprout::launcher {

inline constexpr std::uint32_t kDefaultChildDailyAllowanceSeconds = 45U * 60U;

enum class DailyTimeNotice {
  TenMinutesRemaining,
  FiveMinutesRemaining,
  OneMinuteRemaining,
};

struct TimePolicySample {
  std::uint64_t monotonic_milliseconds;
  std::int64_t utc_seconds;
  std::string local_date;
};

struct DailyTimeStatus {
  std::uint64_t allowance_milliseconds;
  std::uint64_t used_milliseconds;
  std::uint64_t remaining_milliseconds;
  bool expired;
};

struct DailyTimeDecision {
  DailyTimeStatus status;
  std::vector<DailyTimeNotice> notices;
  bool launch_allowed;
  bool save_and_exit_required;
};

class DailyTimePolicyStore {
 public:
  explicit DailyTimePolicyStore(std::filesystem::path database_path);
  ~DailyTimePolicyStore();

  DailyTimePolicyStore(const DailyTimePolicyStore&) = delete;
  DailyTimePolicyStore& operator=(const DailyTimePolicyStore&) = delete;
  DailyTimePolicyStore(DailyTimePolicyStore&&) noexcept;
  DailyTimePolicyStore& operator=(DailyTimePolicyStore&&) noexcept;

  void set_daily_allowance(const std::string& profile_id,
                           std::uint32_t allowance_seconds);
  [[nodiscard]] std::optional<std::uint32_t> find_daily_allowance_seconds(
      const std::string& profile_id) const;
  void remove_unused_daily_allowance(const std::string& profile_id);
  [[nodiscard]] DailyTimeStatus status(const std::string& profile_id,
                                       const TimePolicySample& sample);
  [[nodiscard]] DailyTimeDecision begin_session(
      const std::string& profile_id, const std::string& session_id,
      const std::string& item_id, const TimePolicySample& sample);
  [[nodiscard]] DailyTimeDecision checkpoint_session(
      const std::string& session_id, const TimePolicySample& sample);
  [[nodiscard]] DailyTimeDecision pause_session(
      const std::string& session_id, const TimePolicySample& sample);
  [[nodiscard]] std::optional<DailyTimeDecision> recover_interrupted_session(
      const TimePolicySample& sample);

 private:
  [[nodiscard]] DailyTimeDecision update_active_session(
      const std::string& session_id, const TimePolicySample& sample,
      bool keep_active);

  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace sprout::launcher
