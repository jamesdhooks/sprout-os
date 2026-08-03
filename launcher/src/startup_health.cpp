#include "sprout/launcher/startup_health.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace sprout::launcher {
namespace {

class Statement {
 public:
  Statement(sqlite3* database, const char* sql) {
    if (sqlite3_prepare_v2(database, sql, -1, &statement_, nullptr) !=
        SQLITE_OK) {
      throw std::runtime_error(sqlite3_errmsg(database));
    }
  }

  ~Statement() { sqlite3_finalize(statement_); }
  Statement(const Statement&) = delete;
  Statement& operator=(const Statement&) = delete;
  [[nodiscard]] sqlite3_stmt* get() const noexcept { return statement_; }

 private:
  sqlite3_stmt* statement_{nullptr};
};

void execute(sqlite3* database, const char* sql) {
  char* error = nullptr;
  if (sqlite3_exec(database, sql, nullptr, nullptr, &error) != SQLITE_OK) {
    const std::string message =
        error == nullptr ? sqlite3_errmsg(database) : error;
    sqlite3_free(error);
    throw std::runtime_error(message);
  }
}

void rollback(sqlite3* database) noexcept {
  (void)sqlite3_exec(database, "ROLLBACK", nullptr, nullptr, nullptr);
}

void step_done(sqlite3* database, sqlite3_stmt* statement) {
  if (sqlite3_step(statement) != SQLITE_DONE) {
    throw std::runtime_error(sqlite3_errmsg(database));
  }
}

struct StoredState {
  std::uint64_t next_attempt_id;
  std::optional<std::uint64_t> active_attempt_id;
  std::uint32_t consecutive_failures;
};

StoredState read_state(sqlite3* database) {
  Statement statement(
      database,
      "SELECT singleton, next_attempt_id, active_attempt_id, "
      "consecutive_failures FROM startup_health");
  if (sqlite3_step(statement.get()) != SQLITE_ROW ||
      sqlite3_column_type(statement.get(), 0) != SQLITE_INTEGER ||
      sqlite3_column_type(statement.get(), 1) != SQLITE_INTEGER ||
      (sqlite3_column_type(statement.get(), 2) != SQLITE_NULL &&
       sqlite3_column_type(statement.get(), 2) != SQLITE_INTEGER) ||
      sqlite3_column_type(statement.get(), 3) != SQLITE_INTEGER) {
    throw std::runtime_error("Startup-health state is missing or malformed");
  }

  const auto singleton = sqlite3_column_int64(statement.get(), 0);
  const auto next = sqlite3_column_int64(statement.get(), 1);
  const bool has_active =
      sqlite3_column_type(statement.get(), 2) == SQLITE_INTEGER;
  const auto active = has_active ? sqlite3_column_int64(statement.get(), 2) : 0;
  const auto failures = sqlite3_column_int64(statement.get(), 3);
  if (sqlite3_step(statement.get()) != SQLITE_DONE || singleton != 1 ||
      next < 1 || failures < 0 ||
      failures > kStartupRecoveryFailureThreshold ||
      (has_active && (active < 1 || active >= next)) ||
      (!has_active && failures != 0)) {
    throw std::runtime_error("Startup-health state is inconsistent");
  }
  return StoredState{
      .next_attempt_id = static_cast<std::uint64_t>(next),
      .active_attempt_id =
          has_active
              ? std::optional<std::uint64_t>(static_cast<std::uint64_t>(active))
              : std::nullopt,
      .consecutive_failures = static_cast<std::uint32_t>(failures),
  };
}

int read_schema_version(sqlite3* database) {
  Statement statement(database, "PRAGMA user_version");
  if (sqlite3_step(statement.get()) != SQLITE_ROW ||
      sqlite3_column_type(statement.get(), 0) != SQLITE_INTEGER) {
    throw std::runtime_error("Could not read startup-health schema version");
  }
  return sqlite3_column_int(statement.get(), 0);
}

bool has_user_tables(sqlite3* database) {
  Statement statement(
      database,
      "SELECT 1 FROM sqlite_master WHERE type = 'table' "
      "AND name NOT LIKE 'sqlite_%' LIMIT 1");
  return sqlite3_step(statement.get()) == SQLITE_ROW;
}

}  // namespace

class StartupHealthStore::Impl {
 public:
  explicit Impl(const std::filesystem::path& database_path) {
    if (!database_path.parent_path().empty()) {
      std::filesystem::create_directories(database_path.parent_path());
    }
    if (sqlite3_open(database_path.string().c_str(), &database_) != SQLITE_OK) {
      const std::string message = sqlite3_errmsg(database_);
      sqlite3_close(database_);
      database_ = nullptr;
      throw std::runtime_error(message);
    }
    try {
      sqlite3_busy_timeout(database_, 5'000);
      const int version = read_schema_version(database_);
      if (version > static_cast<int>(kStartupHealthSchemaVersion)) {
        throw std::runtime_error(
            "Startup-health database uses a newer unsupported schema");
      }
      if (version == 0) {
        if (has_user_tables(database_)) {
          throw std::runtime_error(
              "Unversioned startup-health database is not empty");
        }
        create_schema();
      } else if (version != static_cast<int>(kStartupHealthSchemaVersion)) {
        throw std::runtime_error("Startup-health schema version is invalid");
      }
      (void)read_state(database_);
    } catch (...) {
      sqlite3_close(database_);
      database_ = nullptr;
      throw;
    }
  }

  ~Impl() { sqlite3_close(database_); }

  void create_schema() {
    execute(database_, "BEGIN IMMEDIATE");
    try {
      execute(database_, R"sql(
        CREATE TABLE startup_health (
          singleton INTEGER PRIMARY KEY CHECK (singleton = 1),
          next_attempt_id INTEGER NOT NULL CHECK (next_attempt_id >= 1),
          active_attempt_id INTEGER,
          consecutive_failures INTEGER NOT NULL
            CHECK (consecutive_failures BETWEEN 0 AND 3),
          CHECK (
            active_attempt_id IS NULL OR
            (active_attempt_id >= 1 AND active_attempt_id < next_attempt_id)
          ),
          CHECK (active_attempt_id IS NOT NULL OR consecutive_failures = 0)
        );
        INSERT INTO startup_health (
          singleton, next_attempt_id, active_attempt_id, consecutive_failures
        ) VALUES (1, 1, NULL, 0);
        PRAGMA user_version = 1;
      )sql");
      execute(database_, "COMMIT");
    } catch (...) {
      rollback(database_);
      throw;
    }
  }

  [[nodiscard]] sqlite3* database() const noexcept { return database_; }

 private:
  sqlite3* database_{nullptr};
};

StartupHealthStore::StartupHealthStore(std::filesystem::path database_path)
    : impl_(std::make_unique<Impl>(database_path)) {}

StartupHealthStore::~StartupHealthStore() = default;
StartupHealthStore::StartupHealthStore(StartupHealthStore&&) noexcept = default;
StartupHealthStore& StartupHealthStore::operator=(
    StartupHealthStore&&) noexcept = default;

std::uint32_t StartupHealthStore::database_schema_version() const {
  return static_cast<std::uint32_t>(read_schema_version(impl_->database()));
}

StartupDecision StartupHealthStore::begin_startup() {
  sqlite3* database = impl_->database();
  execute(database, "BEGIN IMMEDIATE");
  try {
    const StoredState state = read_state(database);
    if (state.next_attempt_id >=
        static_cast<std::uint64_t>(
            std::numeric_limits<sqlite3_int64>::max())) {
      throw std::runtime_error("Startup attempt identity is exhausted");
    }
    const std::uint32_t failures = state.active_attempt_id.has_value()
        ? std::min(kStartupRecoveryFailureThreshold,
                   state.consecutive_failures + 1U)
        : state.consecutive_failures;
    Statement update(
        database,
        "UPDATE startup_health SET next_attempt_id = ?, "
        "active_attempt_id = ?, consecutive_failures = ? WHERE singleton = 1");
    const auto attempt = static_cast<sqlite3_int64>(state.next_attempt_id);
    if (sqlite3_bind_int64(update.get(), 1, attempt + 1) != SQLITE_OK ||
        sqlite3_bind_int64(update.get(), 2, attempt) != SQLITE_OK ||
        sqlite3_bind_int(update.get(), 3, static_cast<int>(failures)) !=
            SQLITE_OK) {
      throw std::runtime_error("Could not bind startup-health state");
    }
    step_done(database, update.get());
    execute(database, "COMMIT");
    return StartupDecision{
        .attempt_id = state.next_attempt_id,
        .consecutive_failures = failures,
        .recovery_required =
            failures >= kStartupRecoveryFailureThreshold,
    };
  } catch (...) {
    rollback(database);
    throw;
  }
}

void StartupHealthStore::mark_ready(std::uint64_t attempt_id) {
  if (attempt_id == 0 ||
      attempt_id > static_cast<std::uint64_t>(
                       std::numeric_limits<sqlite3_int64>::max())) {
    throw std::invalid_argument("Startup attempt identity is invalid");
  }
  sqlite3* database = impl_->database();
  execute(database, "BEGIN IMMEDIATE");
  try {
    const StoredState state = read_state(database);
    if (!state.active_attempt_id.has_value() ||
        *state.active_attempt_id != attempt_id) {
      throw std::invalid_argument(
          "Ready acknowledgement does not match the active startup");
    }
    Statement update(
        database,
        "UPDATE startup_health SET active_attempt_id = NULL, "
        "consecutive_failures = 0 WHERE singleton = 1");
    step_done(database, update.get());
    execute(database, "COMMIT");
  } catch (...) {
    rollback(database);
    throw;
  }
}

}  // namespace sprout::launcher
