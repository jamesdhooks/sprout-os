#include "sprout/launcher/daily_time_policy.hpp"

#include <sqlite3.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

using sprout::launcher::DailyTimeNotice;
using sprout::launcher::DailyTimePolicyStore;
using sprout::launcher::TimePolicySample;

void expect(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error(std::string(message));
  }
}

template <typename Operation>
void expect_failure(Operation operation, std::string_view message) {
  try {
    operation();
  } catch (const std::exception&) {
    return;
  }
  throw std::runtime_error(std::string(message));
}

class TemporaryDirectory {
 public:
  TemporaryDirectory() {
    const auto suffix =
        std::chrono::high_resolution_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() /
            ("sprout-time-policy-test-" + std::to_string(suffix));
    std::filesystem::create_directories(path_);
  }
  ~TemporaryDirectory() {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }
  const std::filesystem::path& path() const noexcept { return path_; }

 private:
  std::filesystem::path path_;
};

TimePolicySample sample(std::uint64_t monotonic_milliseconds,
                        std::int64_t utc_seconds,
                        std::string local_date = "2026-08-02") {
  return TimePolicySample{
      .monotonic_milliseconds = monotonic_milliseconds,
      .utc_seconds = utc_seconds,
      .local_date = std::move(local_date),
  };
}

void active_time_only_and_restart_persistence() {
  TemporaryDirectory directory;
  const auto database = directory.path() / "time-policy.sqlite3";
  {
    DailyTimePolicyStore policy(database);
    policy.set_daily_allowance("child-1", 45 * 60);
    const auto began = policy.begin_session(
        "child-1", "session-1", "onion:GB:Nested%2FZelda.GB",
        sample(10'000, 1'000));
    expect(began.launch_allowed, "unused allowance should permit launch");
    const auto checkpoint = policy.checkpoint_session(
        "session-1", sample(130'000, 1'120));
    expect(checkpoint.status.used_milliseconds == 120'000,
           "monotonic active delta should be charged");
    const auto paused =
        policy.pause_session("session-1", sample(160'000, 1'150));
    expect(paused.status.used_milliseconds == 150'000,
           "active time through pause should be charged");
  }
  DailyTimePolicyStore resumed(database);
  const auto after_inactive_time = resumed.status(
      "child-1", sample(1, 10'000));
  expect(after_inactive_time.used_milliseconds == 150'000,
         "inactive launcher time and restart should not consume allowance");
}

void warnings_emit_once_at_crossed_thresholds() {
  TemporaryDirectory directory;
  const auto database = directory.path() / "time-policy.sqlite3";
  {
    DailyTimePolicyStore policy(database);
    policy.set_daily_allowance("child-1", 11 * 60);
    (void)policy.begin_session("child-1", "session-1", "game.gb",
                               sample(0, 1'000));
    const auto update =
        policy.pause_session("session-1", sample(60'000, 1'060));
    expect(update.notices.size() == 1 &&
               update.notices.front() == DailyTimeNotice::TenMinutesRemaining,
           "crossing ten minutes should emit its warning");
  }
  DailyTimePolicyStore policy(database);
  (void)policy.begin_session("child-1", "session-2", "game.gb",
                             sample(0, 1'061));
  auto update =
      policy.checkpoint_session("session-2", sample(1'000, 1'062));
  expect(update.notices.empty(),
         "a warning should remain acknowledged across restart");
  update = policy.checkpoint_session("session-2", sample(300'000, 1'361));
  expect(update.notices.size() == 1 &&
             update.notices.front() == DailyTimeNotice::FiveMinutesRemaining,
         "crossing five minutes should emit its warning");
  update = policy.checkpoint_session("session-2", sample(540'000, 1'601));
  expect(update.notices.size() == 1 &&
             update.notices.front() == DailyTimeNotice::OneMinuteRemaining,
         "crossing one minute should emit its warning");
}

void expiration_blocks_resume_and_requests_normal_exit() {
  TemporaryDirectory directory;
  DailyTimePolicyStore policy(directory.path() / "time-policy.sqlite3");
  policy.set_daily_allowance("child-1", 60);
  (void)policy.begin_session("child-1", "session-1", "game.gb",
                             sample(1'000, 1'000));
  const auto expired = policy.checkpoint_session(
      "session-1", sample(61'000, 1'060));
  expect(expired.status.expired && expired.save_and_exit_required &&
             !expired.launch_allowed,
         "zero allowance should request normal save and exit");
  const auto blocked = policy.begin_session(
      "child-1", "session-2", "game.gb", sample(62'000, 1'061));
  expect(!blocked.launch_allowed && !blocked.save_and_exit_required,
         "expired allowance should block a new launch or resume");
}

void interrupted_session_recovery_is_bounded() {
  TemporaryDirectory directory;
  const auto database = directory.path() / "time-policy.sqlite3";
  {
    DailyTimePolicyStore policy(database);
    policy.set_daily_allowance("child-1", 60 * 60);
    (void)policy.begin_session("child-1", "session-1", "game.gb",
                               sample(50'000, 1'000));
    (void)policy.checkpoint_session("session-1", sample(60'000, 1'010));
  }
  DailyTimePolicyStore resumed(database);
  const auto recovered =
      resumed.recover_interrupted_session(sample(1'000, 10'000));
  expect(recovered.has_value() &&
             recovered->status.used_milliseconds == 40'000,
         "restart recovery should cap uncheckpointed time at thirty seconds");
  expect(!resumed.recover_interrupted_session(sample(2'000, 10'001)).has_value(),
         "recovered active marker should be cleared exactly once");
}

void day_rollover_and_clock_rollback_fail_closed() {
  TemporaryDirectory directory;
  const auto database = directory.path() / "time-policy.sqlite3";
  DailyTimePolicyStore policy(database);
  policy.set_daily_allowance("child-1", 60);
  (void)policy.begin_session("child-1", "session-1", "game.gb",
                             sample(0, 1'000));
  (void)policy.pause_session("session-1", sample(30'000, 1'030));
  const auto next_day = policy.status(
      "child-1", sample(1, 2'000, "2026-08-03"));
  expect(next_day.used_milliseconds == 0 &&
             next_day.remaining_milliseconds == 60'000,
         "a forward local-date transition should select a fresh daily budget");
  expect_failure(
      [&] {
        (void)policy.status("child-1", sample(2, 1'999, "2026-08-03"));
      },
      "wall-clock rollback should fail closed");
  expect_failure(
      [&] {
        (void)policy.status("child-1", sample(3, 2'001, "2026-08-02"));
      },
      "local-date rollback should fail closed");
}

void active_midnight_transition_requires_normal_exit() {
  TemporaryDirectory directory;
  DailyTimePolicyStore policy(directory.path() / "time-policy.sqlite3");
  policy.set_daily_allowance("child-1", 60);
  (void)policy.begin_session("child-1", "session-1", "game.gb",
                             sample(0, 1'000));
  const auto rollover = policy.checkpoint_session(
      "session-1", sample(5'000, 1'005, "2026-08-03"));
  expect(rollover.save_and_exit_required && !rollover.launch_allowed,
         "an active local-date transition should return through normal exit");
  expect(rollover.status.used_milliseconds == 0 &&
             rollover.status.remaining_milliseconds == 60'000,
         "the new local date should expose a fresh budget after exit");
}

void invalid_inputs_and_newer_schema_are_rejected() {
  TemporaryDirectory directory;
  const auto database = directory.path() / "time-policy.sqlite3";
  {
    DailyTimePolicyStore policy(database);
    expect_failure([&] { policy.set_daily_allowance("child-1", 0); },
                   "zero allowance should be rejected");
    policy.set_daily_allowance("child-1", 60);
    expect_failure(
        [&] {
          (void)policy.status("child-1", sample(0, 1'000, "2026-02-30"));
        },
        "invalid calendar dates should be rejected");
    (void)policy.begin_session("child-1", "session-1", "game.gb",
                               sample(10, 1'000));
    expect_failure(
        [&] {
          (void)policy.checkpoint_session("session-1", sample(9, 1'001));
        },
        "monotonic rollback should be rejected");
  }

  sqlite3* raw = nullptr;
  expect(sqlite3_open(database.string().c_str(), &raw) == SQLITE_OK,
         "test should open time-policy schema metadata");
  expect(sqlite3_exec(raw, "PRAGMA user_version = 2", nullptr, nullptr, nullptr) ==
             SQLITE_OK,
         "test should set a future schema version");
  sqlite3_close(raw);
  expect_failure([&] { DailyTimePolicyStore newer(database); },
                 "newer time-policy schema should be rejected");
}

void unused_allowance_can_be_compensated_safely() {
  TemporaryDirectory directory;
  DailyTimePolicyStore policy(directory.path() / "time-policy.sqlite3");
  policy.set_daily_allowance("child-1", 45 * 60);
  expect(policy.find_daily_allowance_seconds("child-1") == 45 * 60,
         "configured allowance should be available to portable consumers");
  policy.remove_unused_daily_allowance("child-1");
  expect(!policy.find_daily_allowance_seconds("child-1").has_value(),
         "unused allowance compensation should remove its policy row");

  policy.set_daily_allowance("child-1", 45 * 60);
  (void)policy.begin_session("child-1", "session-1", "game.gb",
                             sample(0, 1'000));
  (void)policy.pause_session("session-1", sample(1'000, 1'001));
  expect_failure([&] { policy.remove_unused_daily_allowance("child-1"); },
                 "policy with usage should not be removed as compensation");
}

}  // namespace

int main() {
  try {
    active_time_only_and_restart_persistence();
    warnings_emit_once_at_crossed_thresholds();
    expiration_blocks_resume_and_requests_normal_exit();
    interrupted_session_recovery_is_bounded();
    day_rollover_and_clock_rollback_fail_closed();
    active_midnight_transition_requires_normal_exit();
    invalid_inputs_and_newer_schema_are_rejected();
    unused_allowance_can_be_compensated_safely();
  } catch (const std::exception& error) {
    std::cerr << "daily time policy test failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
  std::cout << "daily time policy tests passed\n";
  return EXIT_SUCCESS;
}
