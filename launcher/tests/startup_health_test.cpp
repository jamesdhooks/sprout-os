#include "sprout/launcher/startup_health.hpp"

#include <sqlite3.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using sprout::launcher::StartupDecision;
using sprout::launcher::StartupHealthStore;

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
            ("sprout-startup-health-test-" + std::to_string(suffix));
    std::filesystem::create_directories(path_);
  }

  ~TemporaryDirectory() {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }

  [[nodiscard]] const std::filesystem::path& path() const noexcept {
    return path_;
  }

 private:
  std::filesystem::path path_;
};

void execute_sql(const std::filesystem::path& path, const char* sql) {
  sqlite3* database = nullptr;
  if (sqlite3_open(path.string().c_str(), &database) != SQLITE_OK) {
    throw std::runtime_error("Could not create startup-health fixture");
  }
  char* error = nullptr;
  if (sqlite3_exec(database, sql, nullptr, nullptr, &error) != SQLITE_OK) {
    const std::string message = error == nullptr ? sqlite3_errmsg(database) : error;
    sqlite3_free(error);
    sqlite3_close(database);
    throw std::runtime_error(message);
  }
  sqlite3_close(database);
}

std::vector<char> bytes(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(stream),
          std::istreambuf_iterator<char>()};
}

void first_start_and_ready_reset_are_persistent() {
  TemporaryDirectory directory;
  const auto database = directory.path() / "data" / "startup-health.sqlite3";
  std::uint64_t first_attempt = 0;
  {
    StartupHealthStore health(database);
    expect(health.database_schema_version() == 1,
           "new store should create schema version one");
    const auto first = health.begin_startup();
    first_attempt = first.attempt_id;
    expect(first.attempt_id == 1 && first.consecutive_failures == 0 &&
               !first.recovery_required,
           "first startup should have no prior failure");
    health.mark_ready(first.attempt_id);
  }
  StartupHealthStore reopened(database);
  const auto next = reopened.begin_startup();
  expect(next.attempt_id > first_attempt && next.consecutive_failures == 0 &&
             !next.recovery_required,
         "ready startup should reset failures across restart");
}

void unfinished_attempts_count_once_and_request_recovery() {
  TemporaryDirectory directory;
  const auto database = directory.path() / "startup-health.sqlite3";
  StartupDecision decision{};
  for (std::uint32_t failure = 0; failure <= 3; ++failure) {
    StartupHealthStore health(database);
    decision = health.begin_startup();
    expect(decision.consecutive_failures == failure,
           "each unfinished prior attempt should count exactly once");
  }
  expect(decision.recovery_required && decision.consecutive_failures == 3,
         "three unfinished attempts should request recovery");

  StartupHealthStore recovered(database);
  recovered.mark_ready(decision.attempt_id);
  const auto next = recovered.begin_startup();
  expect(next.consecutive_failures == 0 && !next.recovery_required,
         "matching ready acknowledgement should clear recovery state");
}

void stale_missing_and_repeated_acknowledgements_fail_closed() {
  TemporaryDirectory directory;
  StartupHealthStore health(directory.path() / "startup-health.sqlite3");
  const auto stale = health.begin_startup();
  const auto active = health.begin_startup();
  expect_failure([&] { health.mark_ready(stale.attempt_id); },
                 "stale attempt must not clear a newer startup");
  expect_failure([&] { health.mark_ready(0); },
                 "missing attempt identity must fail");
  health.mark_ready(active.attempt_id);
  expect_failure([&] { health.mark_ready(active.attempt_id); },
                 "repeated acknowledgement must fail");
  const auto next = health.begin_startup();
  expect(next.consecutive_failures == 0,
         "failed stale acknowledgements must leave the active attempt intact");
}

void newer_and_inconsistent_databases_are_unchanged() {
  TemporaryDirectory directory;
  const auto newer = directory.path() / "newer.sqlite3";
  execute_sql(newer,
              "CREATE TABLE sentinel(value INTEGER); PRAGMA user_version = 2;");
  const auto newer_before = bytes(newer);
  expect_failure([&] { StartupHealthStore health(newer); },
                 "newer schema should be rejected");
  expect(bytes(newer) == newer_before,
         "newer schema rejection should not modify the database");

  const auto inconsistent = directory.path() / "inconsistent.sqlite3";
  execute_sql(inconsistent, R"sql(
    CREATE TABLE startup_health (
      singleton INTEGER,
      next_attempt_id INTEGER,
      active_attempt_id INTEGER,
      consecutive_failures INTEGER
    );
    INSERT INTO startup_health VALUES (1, 2, NULL, 1);
    PRAGMA user_version = 1;
  )sql");
  const auto inconsistent_before = bytes(inconsistent);
  expect_failure([&] { StartupHealthStore health(inconsistent); },
                 "inconsistent schema-v1 state should be rejected");
  expect(bytes(inconsistent) == inconsistent_before,
         "inconsistent state rejection should not modify the database");
}

void unversioned_nonempty_database_is_not_adopted() {
  TemporaryDirectory directory;
  const auto database = directory.path() / "unversioned.sqlite3";
  execute_sql(database, "CREATE TABLE unrelated(value TEXT);");
  const auto before = bytes(database);
  expect_failure([&] { StartupHealthStore health(database); },
                 "unversioned nonempty database should not be adopted");
  expect(bytes(database) == before,
         "rejected unversioned database should remain unchanged");
}

}  // namespace

int main() {
  try {
    first_start_and_ready_reset_are_persistent();
    unfinished_attempts_count_once_and_request_recovery();
    stale_missing_and_repeated_acknowledgements_fail_closed();
    newer_and_inconsistent_databases_are_unchanged();
    unversioned_nonempty_database_is_not_adopted();
  } catch (const std::exception& error) {
    std::cerr << "startup health test failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
  std::cout << "startup health tests passed\n";
  return EXIT_SUCCESS;
}
