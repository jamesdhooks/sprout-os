#include "sprout/launcher/parent_access_store.hpp"
#include "sprout/launcher/string_compat.hpp"

#include <sqlite3.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using sprout::launcher::ParentAccessStore;

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
            ("sprout-parent-access-test-" + std::to_string(suffix));
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

struct AccessFixture {
  TemporaryDirectory directory;
  std::filesystem::path database{directory.path() / "security.sqlite3"};
  std::filesystem::path key{directory.path() / "device-access.key"};
};

void pin_hashes_and_verifies_without_plaintext_storage() {
  AccessFixture fixture;
  ParentAccessStore access(fixture.database, fixture.key);
  access.set_pin("secret:parent-primary", "2468");
  expect(access.verify_pin("secret:parent-primary", "2468"),
         "correct parent PIN should verify");
  expect(!access.verify_pin("secret:parent-primary", "1357"),
         "incorrect parent PIN should fail");
  expect_failure([&] { access.set_pin("secret:parent-primary", "12ab"); },
                 "non-digit PIN should be rejected");

  sqlite3* database = nullptr;
  expect(sqlite3_open(fixture.database.string().c_str(), &database) == SQLITE_OK,
         "test should inspect credential storage");
  sqlite3_stmt* statement = nullptr;
  sqlite3_prepare_v2(database,
                     "SELECT encoded_hash FROM credentials WHERE credential_ref = ?", -1,
                     &statement, nullptr);
  sqlite3_bind_text(statement, 1, "secret:parent-primary", -1, SQLITE_STATIC);
  expect(sqlite3_step(statement) == SQLITE_ROW,
         "credential should have a stored encoded hash");
  const std::string encoded(
      reinterpret_cast<const char*>(sqlite3_column_text(statement, 0)));
  expect(sprout::launcher::starts_with(encoded, "$argon2id$") &&
             encoded.find("2468") == std::string::npos,
         "credential should store only a self-describing Argon2id hash");
  sqlite3_finalize(statement);
  sqlite3_close(database);
}

void end_of_day_grant_survives_restart_and_manual_lock() {
  AccessFixture fixture;
  {
    ParentAccessStore access(fixture.database, fixture.key);
    access.set_pin("secret:parent-primary", "2468");
    access.grant_until_end_of_day("secret:parent-primary", "2468", 1'000,
                                  "2026-08-02");
    expect(access.is_unlocked(1'100, "2026-08-02"),
           "authenticated same-day grant should unlock");
  }
  ParentAccessStore resumed(fixture.database, fixture.key);
  expect(resumed.is_unlocked(1'200, "2026-08-02"),
         "same-day grant should survive process restart");
  resumed.lock();
  expect(!resumed.is_unlocked(1'201, "2026-08-02"),
         "manual lock should revoke immediately");
}

void date_change_and_clock_rollback_fail_closed() {
  AccessFixture date_fixture;
  ParentAccessStore date_access(date_fixture.database, date_fixture.key);
  date_access.set_pin("secret:parent-primary", "2468");
  date_access.grant_until_end_of_day("secret:parent-primary", "2468", 1'000,
                                     "2026-08-02");
  expect(!date_access.is_unlocked(2'000, "2026-08-03"),
         "grant should expire when the local date changes");

  AccessFixture rollback_fixture;
  ParentAccessStore rollback(rollback_fixture.database, rollback_fixture.key);
  rollback.set_pin("secret:parent-primary", "2468");
  rollback.grant_until_end_of_day("secret:parent-primary", "2468", 1'000,
                                  "2026-08-02");
  expect(rollback.is_unlocked(1'200, "2026-08-02"),
         "forward clock movement should retain the grant");
  expect(!rollback.is_unlocked(1'199, "2026-08-02"),
         "clock rollback should revoke the grant");
  expect(!rollback.is_unlocked(1'300, "2026-08-02"),
         "rollback revocation should remain locked");
}

void grant_tampering_fails_closed() {
  AccessFixture fixture;
  ParentAccessStore access(fixture.database, fixture.key);
  access.set_pin("secret:parent-primary", "2468");
  access.grant_until_end_of_day("secret:parent-primary", "2468", 1'000,
                                "2026-08-02");

  sqlite3* database = nullptr;
  expect(sqlite3_open(fixture.database.string().c_str(), &database) == SQLITE_OK,
         "test should open grant storage");
  expect(sqlite3_exec(database, "UPDATE active_grant SET mac = zeroblob(32)", nullptr,
                      nullptr, nullptr) == SQLITE_OK,
         "test should tamper with the persisted grant");
  sqlite3_close(database);
  expect(!access.is_unlocked(1'100, "2026-08-02"),
         "invalid grant authentication should fail closed");
}

void pin_replacement_revokes_and_newer_schema_is_rejected() {
  AccessFixture fixture;
  {
    ParentAccessStore access(fixture.database, fixture.key);
    access.set_pin("secret:parent-primary", "2468");
    access.grant_until_end_of_day("secret:parent-primary", "2468", 1'000,
                                  "2026-08-02");
    access.set_pin("secret:parent-primary", "8642");
    expect(!access.is_unlocked(1'100, "2026-08-02"),
           "changing the PIN should atomically revoke the active grant");
    expect(!access.verify_pin("secret:parent-primary", "2468") &&
               access.verify_pin("secret:parent-primary", "8642"),
           "PIN replacement should activate only the new hash");
    expect_failure([&] { (void)access.is_unlocked(1'100, "2026-02-30"); },
                   "invalid calendar dates should be rejected");
    expect_failure([&] { (void)access.is_unlocked(1'100, "2025-02-29"); },
                   "non-leap February 29 should be rejected");
    expect(!access.is_unlocked(1'100, "2024-02-29"),
           "valid leap dates should be accepted before grant comparison");
  }

  sqlite3* database = nullptr;
  expect(sqlite3_open(fixture.database.string().c_str(), &database) == SQLITE_OK,
         "test should open schema metadata");
  expect(sqlite3_exec(database, "PRAGMA user_version = 2", nullptr, nullptr, nullptr) ==
             SQLITE_OK,
         "test should set a future schema version");
  sqlite3_close(database);
  expect_failure(
      [&] { ParentAccessStore newer(fixture.database, fixture.key); },
      "newer parent-access schema should fail without being overwritten");
}

}  // namespace

int main() {
  try {
    pin_hashes_and_verifies_without_plaintext_storage();
    end_of_day_grant_survives_restart_and_manual_lock();
    date_change_and_clock_rollback_fail_closed();
    grant_tampering_fails_closed();
    pin_replacement_revokes_and_newer_schema_is_rejected();
  } catch (const std::exception& error) {
    std::cerr << "parent access test failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
  std::cout << "parent access tests passed\n";
  return EXIT_SUCCESS;
}
