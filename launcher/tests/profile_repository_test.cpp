#include "sprout/launcher/profile_repository.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using sprout::launcher::NewProfile;
using sprout::launcher::ProfileLifecycle;
using sprout::launcher::ProfileRepository;
using sprout::launcher::ProfileRole;

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

class TemporaryDatabase {
 public:
  TemporaryDatabase() {
    const auto suffix =
        std::chrono::high_resolution_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() /
            ("sprout-profile-test-" + std::to_string(suffix) + ".sqlite3");
  }

  ~TemporaryDatabase() {
    std::error_code ignored;
    std::filesystem::remove(path_, ignored);
    std::filesystem::remove(path_.string() + "-wal", ignored);
    std::filesystem::remove(path_.string() + "-shm", ignored);
  }

  const std::filesystem::path& path() const noexcept { return path_; }

 private:
  std::filesystem::path path_;
};

std::string path_as_utf8(const std::filesystem::path& path) {
  const auto encoded = path.generic_u8string();
  return {reinterpret_cast<const char*>(encoded.data()), encoded.size()};
}

void with_raw_database(const std::filesystem::path& path,
                       const auto& operation) {
  sqlite3* database = nullptr;
  const std::string encoded_path = path_as_utf8(path);
  if (sqlite3_open(encoded_path.c_str(), &database) != SQLITE_OK) {
    sqlite3_close(database);
    throw std::runtime_error("Could not open raw test database");
  }
  try {
    operation(database);
  } catch (...) {
    sqlite3_close(database);
    throw;
  }
  sqlite3_close(database);
}

void raw_execute(sqlite3* database, const char* sql) {
  char* error = nullptr;
  if (sqlite3_exec(database, sql, nullptr, nullptr, &error) == SQLITE_OK) {
    return;
  }
  const std::string message = error == nullptr ? sqlite3_errmsg(database) : error;
  sqlite3_free(error);
  throw std::runtime_error(message);
}

int raw_schema_version(const std::filesystem::path& path) {
  int version = -1;
  with_raw_database(path, [&](sqlite3* database) {
    sqlite3_stmt* statement = nullptr;
    sqlite3_prepare_v2(database, "PRAGMA user_version", -1, &statement, nullptr);
    if (sqlite3_step(statement) == SQLITE_ROW) {
      version = sqlite3_column_int(statement, 0);
    }
    sqlite3_finalize(statement);
  });
  return version;
}

NewProfile parent(std::string id = "parent-sam") {
  return NewProfile{
      .id = std::move(id),
      .display_name = "Sam",
      .role = ProfileRole::Parent,
      .avatar_ref = "builtin:fox",
      .save_namespace = "saves-parent-sam",
  };
}

NewProfile child() {
  return NewProfile{
      .id = "child-alex",
      .display_name = "Alex",
      .role = ProfileRole::Child,
      .avatar_ref = "builtin:sprout",
      .save_namespace = "saves-child-alex",
      .content_policy_ref = "content:child-default",
      .time_policy_ref = "time:child-default",
  };
}

void creates_migrates_and_reloads_household() {
  TemporaryDatabase database;
  {
    ProfileRepository profiles(database.path());
    expect(profiles.database_schema_version() == 2,
           "empty version-zero database should migrate to version two");
    profiles.create_profile(parent());
    profiles.create_profile(child());
  }

  ProfileRepository reloaded(database.path());
  const auto profiles = reloaded.list_profiles();
  expect(profiles.size() == 2, "parent and child should reload from disk");
  expect(profiles[0].schema_version == 1 && profiles[1].schema_version == 1,
         "persisted profiles should use schema version one");
  expect(reloaded.find_profile("parent-sam")->role == ProfileRole::Parent,
         "parent role should survive reload");
  expect(reloaded.find_profile("child-alex")->content_policy_ref.has_value(),
         "child policy reference should survive reload");
}

void updates_profile_backgrounds() {
  TemporaryDatabase database;
  ProfileRepository profiles(database.path());
  profiles.create_profile(parent());
  expect(profiles.find_profile("parent-sam")->background_ref ==
             "builtin:garden-morning",
         "new profiles should start with the safe garden background");
  profiles.set_background_ref("parent-sam", "builtin:treehouse-library");
  const auto updated = profiles.find_profile("parent-sam");
  expect(updated->background_ref == "builtin:treehouse-library" &&
             updated->local_revision == 2,
         "background update should persist and advance the revision");
  expect_failure(
      [&] { profiles.set_background_ref("parent-sam", "local:C:/image.png"); },
      "profile backgrounds should be limited to the built-in catalogue");
}

void archives_and_restores_without_deleting() {
  TemporaryDatabase database;
  ProfileRepository profiles(database.path());
  profiles.create_profile(parent());
  profiles.create_profile(child());

  profiles.archive_profile("child-alex");
  const auto archived = profiles.find_profile("child-alex");
  expect(archived->lifecycle == ProfileLifecycle::Archived,
         "archive should retain the profile in archived state");
  expect(archived->local_revision == 2,
         "archive should advance the local profile revision");
  expect(profiles.list_profiles(false).size() == 1,
         "active profile query should omit archived profiles");

  profiles.restore_profile("child-alex");
  const auto restored = profiles.find_profile("child-alex");
  expect(restored->lifecycle == ProfileLifecycle::Active,
         "restore should reactivate an archived profile");
  expect(restored->local_revision == 3,
         "restore should advance the local profile revision");
}

void protects_the_last_active_parent() {
  TemporaryDatabase database;
  ProfileRepository profiles(database.path());
  profiles.create_profile(parent());
  profiles.create_profile(child());

  expect_failure([&] { profiles.archive_profile("parent-sam"); },
                 "last active parent should not be archivable");
  expect(profiles.find_profile("parent-sam")->lifecycle == ProfileLifecycle::Active,
         "failed archive should roll back the parent lifecycle");

  auto second_parent = parent("parent-riley");
  second_parent.display_name = "Riley";
  second_parent.save_namespace = "saves-parent-riley";
  profiles.create_profile(second_parent);
  profiles.archive_profile("parent-sam");
  expect(profiles.find_profile("parent-sam")->lifecycle ==
             ProfileLifecycle::Archived,
         "a parent may be archived when another active parent remains");
}

void rejects_invalid_profile_values() {
  TemporaryDatabase database;
  ProfileRepository profiles(database.path());
  auto invalid_child = child();
  invalid_child.time_policy_ref.reset();
  expect_failure([&] { profiles.create_profile(invalid_child); },
                 "child without both policies should be rejected");

  auto invalid_json = parent();
  invalid_json.preferences_json = "not-json";
  expect_failure([&] { profiles.create_profile(invalid_json); },
                 "invalid preferences JSON should be rejected");
  expect(profiles.list_profiles().empty(),
         "failed profile writes should not leave partial rows");
}

void updates_only_managed_avatar_references() {
  TemporaryDatabase database;
  ProfileRepository profiles(database.path());
  profiles.create_profile(parent());
  profiles.set_avatar_ref("parent-sam", "local:portrait-2");
  const auto updated = profiles.find_profile("parent-sam");
  expect(updated->avatar_ref == "local:portrait-2",
         "managed avatar reference should be persisted");
  expect(updated->local_revision == 2,
         "avatar update should advance the profile revision");
  expect_failure([&] { profiles.set_avatar_ref("parent-sam", "C:\\private.jpg"); },
                 "raw source path should not be accepted as an avatar reference");
  expect_failure([&] { profiles.set_avatar_ref("missing", "local:portrait"); },
                 "avatar update should reject a missing profile");
}

void rejects_newer_database_versions_without_mutation() {
  TemporaryDatabase database;
  with_raw_database(database.path(), [](sqlite3* raw) {
    raw_execute(raw, "PRAGMA user_version = 99");
  });

  expect_failure([&] { ProfileRepository profiles(database.path()); },
                 "newer database schema should be rejected");
  expect(raw_schema_version(database.path()) == 99,
         "rejected newer schema should remain unchanged");
}

void migrates_version_one_backgrounds_safely() {
  TemporaryDatabase database;
  with_raw_database(database.path(), [](sqlite3* raw) {
    raw_execute(raw, R"sql(
      CREATE TABLE profiles (
        id TEXT PRIMARY KEY,
        role TEXT NOT NULL,
        lifecycle TEXT NOT NULL
      );
      PRAGMA user_version = 1;
    )sql");
  });
  { ProfileRepository profiles(database.path()); }
  expect(raw_schema_version(database.path()) == 2,
         "version-one database should migrate to version two");
  bool found_background = false;
  with_raw_database(database.path(), [&](sqlite3* raw) {
    sqlite3_stmt* statement = nullptr;
    sqlite3_prepare_v2(raw, "PRAGMA table_info(profiles)", -1, &statement, nullptr);
    while (sqlite3_step(statement) == SQLITE_ROW) {
      const auto* name = sqlite3_column_text(statement, 1);
      if (name != nullptr && std::string_view(reinterpret_cast<const char*>(name)) ==
                                 "background_ref") {
        found_background = true;
      }
    }
    sqlite3_finalize(statement);
  });
  expect(found_background,
         "version-one migration should add the background reference column");
}

void rolls_back_failed_migration() {
  TemporaryDatabase database;
  with_raw_database(database.path(), [](sqlite3* raw) {
    raw_execute(raw, "CREATE TABLE profiles (id TEXT)");
  });

  expect_failure([&] { ProfileRepository profiles(database.path()); },
                 "incompatible version-zero fixture should fail migration");
  expect(raw_schema_version(database.path()) == 0,
         "failed migration should roll back the schema version");

  int column_count = 0;
  with_raw_database(database.path(), [&](sqlite3* raw) {
    sqlite3_stmt* statement = nullptr;
    sqlite3_prepare_v2(raw, "PRAGMA table_info(profiles)", -1, &statement, nullptr);
    while (sqlite3_step(statement) == SQLITE_ROW) {
      ++column_count;
    }
    sqlite3_finalize(statement);
  });
  expect(column_count == 1,
         "failed migration should preserve the original fixture table");
}

void schema_excludes_sensitive_profile_data() {
  TemporaryDatabase database;
  ProfileRepository profiles(database.path());
  std::vector<std::string> columns;
  with_raw_database(database.path(), [&](sqlite3* raw) {
    sqlite3_stmt* statement = nullptr;
    sqlite3_prepare_v2(raw, "PRAGMA table_info(profiles)", -1, &statement, nullptr);
    while (sqlite3_step(statement) == SQLITE_ROW) {
      columns.emplace_back(
          reinterpret_cast<const char*>(sqlite3_column_text(statement, 1)));
    }
    sqlite3_finalize(statement);
  });

  for (const std::string_view forbidden :
       {"pin", "pin_hash", "secret", "activity", "play_history", "save_data"}) {
    expect(std::find(columns.begin(), columns.end(), forbidden) == columns.end(),
           "profile table should not contain sensitive or activity data");
  }
}

}  // namespace

int main() {
  try {
    creates_migrates_and_reloads_household();
    archives_and_restores_without_deleting();
    protects_the_last_active_parent();
    rejects_invalid_profile_values();
    updates_only_managed_avatar_references();
    updates_profile_backgrounds();
    rejects_newer_database_versions_without_mutation();
    migrates_version_one_backgrounds_safely();
    rolls_back_failed_migration();
    schema_excludes_sensitive_profile_data();
  } catch (const std::exception& error) {
    std::cerr << "profile repository test failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }

  std::cout << "profile repository tests passed\n";
  return EXIT_SUCCESS;
}
