#include "sprout/launcher/profile_repository.hpp"
#include "sprout/launcher/string_compat.hpp"

#include <sqlite3.h>

#include <stdexcept>
#include <string_view>
#include <utility>

namespace sprout::launcher {
namespace {

class Statement {
 public:
  Statement(sqlite3* database, std::string_view sql) {
    const std::string statement_sql(sql);
    const int result =
        sqlite3_prepare_v2(database, statement_sql.c_str(), -1, &statement_, nullptr);
    if (result != SQLITE_OK) {
      sqlite3_finalize(statement_);
      statement_ = nullptr;
      throw std::runtime_error(sqlite3_errmsg(database));
    }
  }

  ~Statement() { sqlite3_finalize(statement_); }
  Statement(const Statement&) = delete;
  Statement& operator=(const Statement&) = delete;

  sqlite3_stmt* get() const noexcept { return statement_; }

 private:
  sqlite3_stmt* statement_{nullptr};
};

void execute(sqlite3* database, std::string_view sql) {
  char* error = nullptr;
  const std::string statement_sql(sql);
  const int result = sqlite3_exec(database, statement_sql.c_str(), nullptr, nullptr, &error);
  if (result == SQLITE_OK) {
    return;
  }

  const std::string message = error == nullptr ? sqlite3_errmsg(database) : error;
  sqlite3_free(error);
  throw std::runtime_error(message);
}

std::string path_as_utf8(const std::filesystem::path& path) {
  const auto encoded = path.generic_u8string();
  return {reinterpret_cast<const char*>(encoded.data()), encoded.size()};
}

void bind_text(sqlite3_stmt* statement, int index, const std::string& value) {
  if (sqlite3_bind_text(statement, index, value.c_str(), -1, SQLITE_TRANSIENT) !=
      SQLITE_OK) {
    throw std::runtime_error("Could not bind profile text value");
  }
}

void bind_optional_text(sqlite3_stmt* statement, int index,
                        const std::optional<std::string>& value) {
  if (!value.has_value()) {
    if (sqlite3_bind_null(statement, index) != SQLITE_OK) {
      throw std::runtime_error("Could not bind empty profile value");
    }
    return;
  }
  bind_text(statement, index, *value);
}

std::string column_text(sqlite3_stmt* statement, int index) {
  const auto* value = sqlite3_column_text(statement, index);
  if (value == nullptr) {
    throw std::runtime_error("Profile database contains an unexpected null value");
  }
  return reinterpret_cast<const char*>(value);
}

std::optional<std::string> optional_column_text(sqlite3_stmt* statement,
                                                int index) {
  if (sqlite3_column_type(statement, index) == SQLITE_NULL) {
    return std::nullopt;
  }
  return column_text(statement, index);
}

ProfileRole parse_role(const std::string& value) {
  if (value == "parent") {
    return ProfileRole::Parent;
  }
  if (value == "child") {
    return ProfileRole::Child;
  }
  throw std::runtime_error("Profile database contains an unsupported role");
}

std::string_view role_name(ProfileRole role) {
  return role == ProfileRole::Parent ? "parent" : "child";
}

ProfileLifecycle parse_lifecycle(const std::string& value) {
  if (value == "active") {
    return ProfileLifecycle::Active;
  }
  if (value == "archived") {
    return ProfileLifecycle::Archived;
  }
  throw std::runtime_error("Profile database contains an unsupported lifecycle");
}

ProfileRecord read_profile(sqlite3_stmt* statement) {
  const auto schema_version = sqlite3_column_int64(statement, 0);
  if (schema_version != kProfileSchemaVersion) {
    throw std::runtime_error("Profile database contains an unsupported profile schema version");
  }

  return ProfileRecord{
      .schema_version = static_cast<std::uint32_t>(schema_version),
      .id = column_text(statement, 1),
      .display_name = column_text(statement, 2),
      .role = parse_role(column_text(statement, 3)),
      .avatar_ref = column_text(statement, 4),
      .save_namespace = column_text(statement, 5),
      .content_policy_ref = optional_column_text(statement, 6),
      .time_policy_ref = optional_column_text(statement, 7),
      .preferences_json = column_text(statement, 8),
      .lifecycle = parse_lifecycle(column_text(statement, 9)),
      .local_revision = static_cast<std::uint64_t>(sqlite3_column_int64(statement, 10)),
      .created_at = column_text(statement, 11),
      .updated_at = column_text(statement, 12),
  };
}

constexpr std::string_view kProfileColumns =
    "schema_version, id, display_name, role, avatar_ref, save_namespace, "
    "content_policy_ref, time_policy_ref, preferences_json, lifecycle, "
    "local_revision, created_at, updated_at";

void validate_avatar_ref(std::string_view avatar_ref) {
  if (!starts_with(avatar_ref, "builtin:") &&
      !starts_with(avatar_ref, "local:")) {
    throw std::invalid_argument("Avatar reference must use builtin: or local:");
  }
}

void validate_profile(const NewProfile& profile) {
  if (profile.id.empty() || profile.display_name.empty() ||
      profile.avatar_ref.empty() || profile.save_namespace.empty()) {
    throw std::invalid_argument(
        "Profile ID, display name, avatar, and save namespace are required");
  }
  validate_avatar_ref(profile.avatar_ref);
  if (profile.role == ProfileRole::Child &&
      (!profile.content_policy_ref.has_value() ||
       !profile.time_policy_ref.has_value())) {
    throw std::invalid_argument("Child profiles require content and time policies");
  }
}

}  // namespace

class ProfileRepository::Impl {
 public:
  explicit Impl(const std::filesystem::path& database_path) {
    const std::string path = path_as_utf8(database_path);
    const int result = sqlite3_open_v2(path.c_str(), &database_,
                                       SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                                           SQLITE_OPEN_FULLMUTEX,
                                       nullptr);
    if (result != SQLITE_OK) {
      const std::string message =
          database_ == nullptr ? "Could not open profile database"
                               : sqlite3_errmsg(database_);
      sqlite3_close(database_);
      database_ = nullptr;
      throw std::runtime_error(message);
    }

    try {
      sqlite3_busy_timeout(database_, 5000);
      execute(database_, "PRAGMA foreign_keys = ON");
      migrate();
    } catch (...) {
      sqlite3_close(database_);
      database_ = nullptr;
      throw;
    }
  }

  ~Impl() { sqlite3_close(database_); }

  sqlite3* database() const noexcept { return database_; }

  std::uint32_t schema_version() const {
    Statement statement(database_, "PRAGMA user_version");
    if (sqlite3_step(statement.get()) != SQLITE_ROW) {
      throw std::runtime_error("Could not read profile database schema version");
    }
    return static_cast<std::uint32_t>(sqlite3_column_int64(statement.get(), 0));
  }

 private:
  void migrate() {
    const auto version = schema_version();
    if (version > kProfileDatabaseSchemaVersion) {
      throw std::runtime_error("Profile database schema is newer than this Sprout build");
    }
    if (version == kProfileDatabaseSchemaVersion) {
      return;
    }

    execute(database_, "BEGIN IMMEDIATE");
    try {
      execute(database_, R"sql(
        CREATE TABLE IF NOT EXISTS profiles (
          schema_version INTEGER NOT NULL CHECK (schema_version = 1),
          id TEXT PRIMARY KEY NOT NULL CHECK (length(id) > 0),
          display_name TEXT NOT NULL CHECK (length(display_name) > 0),
          role TEXT NOT NULL CHECK (role IN ('parent', 'child')),
          avatar_ref TEXT NOT NULL CHECK (
            avatar_ref LIKE 'builtin:%' OR avatar_ref LIKE 'local:%'
          ),
          save_namespace TEXT NOT NULL UNIQUE CHECK (length(save_namespace) > 0),
          content_policy_ref TEXT,
          time_policy_ref TEXT,
          preferences_json TEXT NOT NULL DEFAULT '{}' CHECK (json_valid(preferences_json)),
          lifecycle TEXT NOT NULL DEFAULT 'active'
            CHECK (lifecycle IN ('active', 'archived')),
          local_revision INTEGER NOT NULL DEFAULT 1 CHECK (local_revision > 0),
          created_at TEXT NOT NULL,
          updated_at TEXT NOT NULL,
          CHECK (
            role = 'parent' OR
            (content_policy_ref IS NOT NULL AND time_policy_ref IS NOT NULL)
          )
        )
      )sql");
      execute(database_,
              "CREATE INDEX IF NOT EXISTS profiles_lifecycle_index "
              "ON profiles(lifecycle, role)");
      execute(database_, "PRAGMA user_version = 1");
      execute(database_, "COMMIT");
    } catch (...) {
      try {
        execute(database_, "ROLLBACK");
      } catch (...) {
      }
      throw;
    }
  }

  sqlite3* database_{nullptr};
};

ProfileRepository::ProfileRepository(const std::filesystem::path& database_path)
    : impl_(std::make_unique<Impl>(database_path)) {}

ProfileRepository::~ProfileRepository() = default;
ProfileRepository::ProfileRepository(ProfileRepository&&) noexcept = default;
ProfileRepository& ProfileRepository::operator=(ProfileRepository&&) noexcept = default;

std::uint32_t ProfileRepository::database_schema_version() const {
  return impl_->schema_version();
}

std::vector<ProfileRecord> ProfileRepository::list_profiles(
    bool include_archived) const {
  const std::string sql = "SELECT " + std::string(kProfileColumns) +
                          " FROM profiles" +
                          (include_archived ? "" : " WHERE lifecycle = 'active'") +
                          " ORDER BY created_at, id";
  Statement statement(impl_->database(), sql);
  std::vector<ProfileRecord> profiles;
  int result = SQLITE_ROW;
  while ((result = sqlite3_step(statement.get())) == SQLITE_ROW) {
    profiles.push_back(read_profile(statement.get()));
  }
  if (result != SQLITE_DONE) {
    throw std::runtime_error(sqlite3_errmsg(impl_->database()));
  }
  return profiles;
}

std::optional<ProfileRecord> ProfileRepository::find_profile(
    const std::string& id) const {
  const std::string sql = "SELECT " + std::string(kProfileColumns) +
                          " FROM profiles WHERE id = ?";
  Statement statement(impl_->database(), sql);
  bind_text(statement.get(), 1, id);
  const int result = sqlite3_step(statement.get());
  if (result == SQLITE_DONE) {
    return std::nullopt;
  }
  if (result != SQLITE_ROW) {
    throw std::runtime_error(sqlite3_errmsg(impl_->database()));
  }
  return read_profile(statement.get());
}

void ProfileRepository::create_profile(const NewProfile& profile) {
  validate_profile(profile);
  Statement statement(impl_->database(), R"sql(
    INSERT INTO profiles (
      schema_version, id, display_name, role, avatar_ref, save_namespace,
      content_policy_ref, time_policy_ref, preferences_json, lifecycle,
      local_revision, created_at, updated_at
    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 'active', 1,
              strftime('%Y-%m-%dT%H:%M:%fZ', 'now'),
              strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))
  )sql");
  sqlite3_bind_int64(statement.get(), 1, kProfileSchemaVersion);
  bind_text(statement.get(), 2, profile.id);
  bind_text(statement.get(), 3, profile.display_name);
  bind_text(statement.get(), 4, std::string(role_name(profile.role)));
  bind_text(statement.get(), 5, profile.avatar_ref);
  bind_text(statement.get(), 6, profile.save_namespace);
  bind_optional_text(statement.get(), 7, profile.content_policy_ref);
  bind_optional_text(statement.get(), 8, profile.time_policy_ref);
  bind_text(statement.get(), 9, profile.preferences_json);
  if (sqlite3_step(statement.get()) != SQLITE_DONE) {
    throw std::runtime_error(sqlite3_errmsg(impl_->database()));
  }
}

void ProfileRepository::set_avatar_ref(const std::string& id,
                                       const std::string& avatar_ref) {
  validate_avatar_ref(avatar_ref);
  Statement statement(impl_->database(), R"sql(
    UPDATE profiles
    SET avatar_ref = ?, local_revision = local_revision + 1,
        updated_at = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
    WHERE id = ?
  )sql");
  bind_text(statement.get(), 1, avatar_ref);
  bind_text(statement.get(), 2, id);
  if (sqlite3_step(statement.get()) != SQLITE_DONE) {
    throw std::runtime_error(sqlite3_errmsg(impl_->database()));
  }
  if (sqlite3_changes(impl_->database()) == 0) {
    throw std::invalid_argument("Profile does not exist");
  }
}

void ProfileRepository::archive_profile(const std::string& id) {
  execute(impl_->database(), "BEGIN IMMEDIATE");
  try {
    const auto profile = find_profile(id);
    if (!profile.has_value()) {
      throw std::invalid_argument("Profile does not exist");
    }
    if (profile->lifecycle == ProfileLifecycle::Archived) {
      execute(impl_->database(), "COMMIT");
      return;
    }
    if (profile->role == ProfileRole::Parent) {
      Statement count(impl_->database(),
                      "SELECT count(*) FROM profiles "
                      "WHERE role = 'parent' AND lifecycle = 'active'");
      if (sqlite3_step(count.get()) != SQLITE_ROW ||
          sqlite3_column_int64(count.get(), 0) <= 1) {
        throw std::runtime_error("Cannot archive the last active parent profile");
      }
    }

    Statement update(impl_->database(), R"sql(
      UPDATE profiles
      SET lifecycle = 'archived', local_revision = local_revision + 1,
          updated_at = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
      WHERE id = ?
    )sql");
    bind_text(update.get(), 1, id);
    if (sqlite3_step(update.get()) != SQLITE_DONE) {
      throw std::runtime_error(sqlite3_errmsg(impl_->database()));
    }
    execute(impl_->database(), "COMMIT");
  } catch (...) {
    try {
      execute(impl_->database(), "ROLLBACK");
    } catch (...) {
    }
    throw;
  }
}

void ProfileRepository::restore_profile(const std::string& id) {
  Statement statement(impl_->database(), R"sql(
    UPDATE profiles
    SET lifecycle = 'active', local_revision = local_revision + 1,
        updated_at = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
    WHERE id = ? AND lifecycle = 'archived'
  )sql");
  bind_text(statement.get(), 1, id);
  if (sqlite3_step(statement.get()) != SQLITE_DONE) {
    throw std::runtime_error(sqlite3_errmsg(impl_->database()));
  }
  if (sqlite3_changes(impl_->database()) == 0 && !find_profile(id).has_value()) {
    throw std::invalid_argument("Profile does not exist");
  }
}

}  // namespace sprout::launcher
