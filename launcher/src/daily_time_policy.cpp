#include "sprout/launcher/daily_time_policy.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace sprout::launcher {
namespace {

constexpr std::uint64_t kMillisecondsPerSecond = 1'000;
constexpr std::uint64_t kMaximumAllowanceSeconds = 24 * 60 * 60;
constexpr std::uint64_t kRecoveryCheckpointMilliseconds = 30'000;
struct WarningThreshold {
  std::uint64_t remaining_milliseconds;
  DailyTimeNotice notice;
  unsigned mask;
};

constexpr std::array<WarningThreshold, 3> kWarningThresholds{
    WarningThreshold{10 * 60 * kMillisecondsPerSecond,
                     DailyTimeNotice::TenMinutesRemaining, 1U << 0},
    WarningThreshold{5 * 60 * kMillisecondsPerSecond,
                     DailyTimeNotice::FiveMinutesRemaining, 1U << 1},
    WarningThreshold{1 * 60 * kMillisecondsPerSecond,
                     DailyTimeNotice::OneMinuteRemaining, 1U << 2},
};

class Statement {
 public:
  Statement(sqlite3* database, const char* sql) {
    if (sqlite3_prepare_v2(database, sql, -1, &statement_, nullptr) != SQLITE_OK) {
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

void execute(sqlite3* database, const char* sql) {
  char* error = nullptr;
  if (sqlite3_exec(database, sql, nullptr, nullptr, &error) != SQLITE_OK) {
    const std::string message = error == nullptr ? sqlite3_errmsg(database) : error;
    sqlite3_free(error);
    throw std::runtime_error(message);
  }
}

void bind_text(sqlite3_stmt* statement, int index, const std::string& value) {
  if (sqlite3_bind_text(statement, index, value.c_str(),
                        static_cast<int>(value.size()), SQLITE_TRANSIENT) != SQLITE_OK) {
    throw std::runtime_error("Could not bind time-policy text");
  }
}

void bind_u64(sqlite3_stmt* statement, int index, std::uint64_t value) {
  if (value > static_cast<std::uint64_t>(std::numeric_limits<sqlite3_int64>::max()) ||
      sqlite3_bind_int64(statement, index, static_cast<sqlite3_int64>(value)) !=
          SQLITE_OK) {
    throw std::invalid_argument("Time-policy duration is too large");
  }
}

void step_done(sqlite3* database, sqlite3_stmt* statement) {
  if (sqlite3_step(statement) != SQLITE_DONE) {
    throw std::runtime_error(sqlite3_errmsg(database));
  }
}

void validate_identifier(std::string_view value, std::string_view field) {
  if (value.empty() || value.size() > 4'096 ||
      std::any_of(value.begin(), value.end(), [](unsigned char character) {
        return character < 0x20 || character == 0x7f;
      })) {
    throw std::invalid_argument(std::string(field) + " is invalid");
  }
}

void validate_local_date(std::string_view value) {
  if (value.size() != 10 || value[4] != '-' || value[7] != '-') {
    throw std::invalid_argument("Local date must use YYYY-MM-DD");
  }
  for (std::size_t index = 0; index < value.size(); ++index) {
    if (index != 4 && index != 7 &&
        std::isdigit(static_cast<unsigned char>(value[index])) == 0) {
      throw std::invalid_argument("Local date must use YYYY-MM-DD");
    }
  }
  const int year = std::stoi(std::string(value.substr(0, 4)));
  const unsigned month =
      static_cast<unsigned>(std::stoi(std::string(value.substr(5, 2))));
  const unsigned day =
      static_cast<unsigned>(std::stoi(std::string(value.substr(8, 2))));
  static constexpr std::array<unsigned, 12> days_per_month{
      31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (year < 1 || month < 1 || month > days_per_month.size()) {
    throw std::invalid_argument("Local date is not a calendar date");
  }
  unsigned maximum_day = days_per_month[month - 1];
  if (month == 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) {
    ++maximum_day;
  }
  if (day < 1 || day > maximum_day) {
    throw std::invalid_argument("Local date is not a calendar date");
  }
}

void validate_sample(const TimePolicySample& sample) {
  validate_local_date(sample.local_date);
  if (sample.utc_seconds < 0) {
    throw std::invalid_argument("UTC time cannot be negative");
  }
}

struct StoredUsage {
  std::uint64_t used_milliseconds{0};
  unsigned warning_mask{0};
};

struct ActiveSession {
  std::string profile_id;
  std::string session_id;
  std::string item_id;
  std::string local_date;
  std::uint64_t last_monotonic_milliseconds;
  std::int64_t last_utc_seconds;
};

DailyTimeStatus make_status(std::uint64_t allowance, std::uint64_t used) {
  const auto bounded_used = std::min(allowance, used);
  const auto remaining = allowance - bounded_used;
  return DailyTimeStatus{
      .allowance_milliseconds = allowance,
      .used_milliseconds = bounded_used,
      .remaining_milliseconds = remaining,
      .expired = remaining == 0,
  };
}

}  // namespace

class DailyTimePolicyStore::Impl {
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
      execute(database_, "PRAGMA foreign_keys = ON");
      const int schema_version = read_schema_version();
      if (schema_version > 1) {
        throw std::runtime_error(
            "Time-policy database uses a newer unsupported schema");
      }
      execute(database_, R"sql(
        CREATE TABLE IF NOT EXISTS daily_policy (
          profile_id TEXT PRIMARY KEY NOT NULL,
          allowance_milliseconds INTEGER NOT NULL
            CHECK (allowance_milliseconds > 0 AND allowance_milliseconds <= 86400000)
        );
        CREATE TABLE IF NOT EXISTS daily_usage (
          profile_id TEXT NOT NULL,
          local_date TEXT NOT NULL,
          used_milliseconds INTEGER NOT NULL DEFAULT 0 CHECK (used_milliseconds >= 0),
          warning_mask INTEGER NOT NULL DEFAULT 0 CHECK (warning_mask BETWEEN 0 AND 7),
          PRIMARY KEY (profile_id, local_date),
          FOREIGN KEY (profile_id) REFERENCES daily_policy(profile_id)
        );
        CREATE TABLE IF NOT EXISTS profile_clock (
          profile_id TEXT PRIMARY KEY NOT NULL,
          last_local_date TEXT NOT NULL,
          last_utc_seconds INTEGER NOT NULL CHECK (last_utc_seconds >= 0),
          FOREIGN KEY (profile_id) REFERENCES daily_policy(profile_id)
        );
        CREATE TABLE IF NOT EXISTS active_session (
          singleton INTEGER PRIMARY KEY CHECK (singleton = 1),
          profile_id TEXT NOT NULL,
          session_id TEXT NOT NULL UNIQUE,
          item_id TEXT NOT NULL,
          local_date TEXT NOT NULL,
          last_monotonic_milliseconds INTEGER NOT NULL
            CHECK (last_monotonic_milliseconds >= 0),
          last_utc_seconds INTEGER NOT NULL CHECK (last_utc_seconds >= 0),
          FOREIGN KEY (profile_id) REFERENCES daily_policy(profile_id)
        );
      )sql");
      if (schema_version == 0) {
        execute(database_, "PRAGMA user_version = 1");
      }
    } catch (...) {
      sqlite3_close(database_);
      database_ = nullptr;
      throw;
    }
  }

  ~Impl() { sqlite3_close(database_); }

  int read_schema_version() const {
    Statement statement(database_, "PRAGMA user_version");
    if (sqlite3_step(statement.get()) != SQLITE_ROW) {
      throw std::runtime_error("Could not read time-policy schema version");
    }
    return sqlite3_column_int(statement.get(), 0);
  }

  std::uint64_t allowance(const std::string& profile_id) const {
    Statement statement(database_,
                        "SELECT allowance_milliseconds FROM daily_policy "
                        "WHERE profile_id = ?");
    bind_text(statement.get(), 1, profile_id);
    if (sqlite3_step(statement.get()) != SQLITE_ROW) {
      throw std::invalid_argument("Profile has no daily time policy");
    }
    return static_cast<std::uint64_t>(sqlite3_column_int64(statement.get(), 0));
  }

  std::optional<std::uint64_t> find_allowance(
      const std::string& profile_id) const {
    Statement statement(database_,
                        "SELECT allowance_milliseconds FROM daily_policy "
                        "WHERE profile_id = ?");
    bind_text(statement.get(), 1, profile_id);
    if (sqlite3_step(statement.get()) != SQLITE_ROW) {
      return std::nullopt;
    }
    return static_cast<std::uint64_t>(sqlite3_column_int64(statement.get(), 0));
  }

  StoredUsage usage(const std::string& profile_id,
                    const std::string& local_date) const {
    Statement statement(database_,
                        "SELECT used_milliseconds, warning_mask FROM daily_usage "
                        "WHERE profile_id = ? AND local_date = ?");
    bind_text(statement.get(), 1, profile_id);
    bind_text(statement.get(), 2, local_date);
    if (sqlite3_step(statement.get()) != SQLITE_ROW) {
      return {};
    }
    return StoredUsage{
        .used_milliseconds =
            static_cast<std::uint64_t>(sqlite3_column_int64(statement.get(), 0)),
        .warning_mask =
            static_cast<unsigned>(sqlite3_column_int(statement.get(), 1)),
    };
  }

  void observe_clock(const std::string& profile_id,
                     const TimePolicySample& sample) {
    Statement read(database_,
                   "SELECT last_local_date, last_utc_seconds FROM profile_clock "
                   "WHERE profile_id = ?");
    bind_text(read.get(), 1, profile_id);
    if (sqlite3_step(read.get()) == SQLITE_ROW) {
      const std::string previous_date(
          reinterpret_cast<const char*>(sqlite3_column_text(read.get(), 0)));
      const auto previous_utc = sqlite3_column_int64(read.get(), 1);
      if (sample.local_date < previous_date || sample.utc_seconds < previous_utc) {
        throw std::invalid_argument("Time-policy clock rollback detected");
      }
    }
    Statement write(database_, R"sql(
      INSERT INTO profile_clock (profile_id, last_local_date, last_utc_seconds)
      VALUES (?, ?, ?)
      ON CONFLICT(profile_id) DO UPDATE SET
        last_local_date = excluded.last_local_date,
        last_utc_seconds = excluded.last_utc_seconds
    )sql");
    bind_text(write.get(), 1, profile_id);
    bind_text(write.get(), 2, sample.local_date);
    sqlite3_bind_int64(write.get(), 3, sample.utc_seconds);
    step_done(database_, write.get());
  }

  std::optional<ActiveSession> active_session() const {
    Statement statement(database_, R"sql(
      SELECT profile_id, session_id, item_id, local_date,
             last_monotonic_milliseconds, last_utc_seconds
      FROM active_session WHERE singleton = 1
    )sql");
    if (sqlite3_step(statement.get()) != SQLITE_ROW) {
      return std::nullopt;
    }
    return ActiveSession{
        .profile_id = reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 0)),
        .session_id = reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 1)),
        .item_id = reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 2)),
        .local_date = reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 3)),
        .last_monotonic_milliseconds =
            static_cast<std::uint64_t>(sqlite3_column_int64(statement.get(), 4)),
        .last_utc_seconds = sqlite3_column_int64(statement.get(), 5),
    };
  }

  DailyTimeDecision charge(const ActiveSession& active, std::uint64_t delta,
                           bool keep_active,
                           const TimePolicySample& sample) {
    const auto daily_allowance = allowance(active.profile_id);
    auto stored = usage(active.profile_id, active.local_date);
    const auto previous = make_status(daily_allowance, stored.used_milliseconds);
    const auto available_delta = previous.remaining_milliseconds;
    const auto charged_delta = std::min(delta, available_delta);
    stored.used_milliseconds += charged_delta;
    const auto current = make_status(daily_allowance, stored.used_milliseconds);

    std::vector<DailyTimeNotice> notices;
    for (const auto& warning : kWarningThresholds) {
      if ((stored.warning_mask & warning.mask) == 0 &&
          previous.remaining_milliseconds >= warning.remaining_milliseconds &&
          current.remaining_milliseconds <= warning.remaining_milliseconds &&
          previous.remaining_milliseconds != current.remaining_milliseconds) {
        stored.warning_mask |= warning.mask;
        notices.push_back(warning.notice);
      }
    }

    Statement usage_write(database_, R"sql(
      INSERT INTO daily_usage
        (profile_id, local_date, used_milliseconds, warning_mask)
      VALUES (?, ?, ?, ?)
      ON CONFLICT(profile_id, local_date) DO UPDATE SET
        used_milliseconds = excluded.used_milliseconds,
        warning_mask = excluded.warning_mask
    )sql");
    bind_text(usage_write.get(), 1, active.profile_id);
    bind_text(usage_write.get(), 2, active.local_date);
    bind_u64(usage_write.get(), 3, stored.used_milliseconds);
    sqlite3_bind_int(usage_write.get(), 4, static_cast<int>(stored.warning_mask));
    step_done(database_, usage_write.get());

    if (keep_active && !current.expired && sample.local_date == active.local_date) {
      Statement update(database_, R"sql(
        UPDATE active_session
        SET last_monotonic_milliseconds = ?, last_utc_seconds = ?
        WHERE singleton = 1
      )sql");
      bind_u64(update.get(), 1, sample.monotonic_milliseconds);
      sqlite3_bind_int64(update.get(), 2, sample.utc_seconds);
      step_done(database_, update.get());
    } else {
      execute(database_, "DELETE FROM active_session WHERE singleton = 1");
    }

    const bool date_changed = sample.local_date != active.local_date;
    return DailyTimeDecision{
        .status = date_changed
                      ? make_status(daily_allowance,
                                    usage(active.profile_id, sample.local_date)
                                        .used_milliseconds)
                      : current,
        .notices = std::move(notices),
        .launch_allowed = !current.expired && !date_changed,
        .save_and_exit_required = current.expired || date_changed,
    };
  }

  sqlite3* database_{nullptr};
};

DailyTimePolicyStore::DailyTimePolicyStore(std::filesystem::path database_path)
    : impl_(std::make_unique<Impl>(database_path)) {}

DailyTimePolicyStore::~DailyTimePolicyStore() = default;
DailyTimePolicyStore::DailyTimePolicyStore(DailyTimePolicyStore&&) noexcept = default;
DailyTimePolicyStore& DailyTimePolicyStore::operator=(
    DailyTimePolicyStore&&) noexcept = default;

void DailyTimePolicyStore::set_daily_allowance(const std::string& profile_id,
                                                std::uint32_t allowance_seconds) {
  validate_identifier(profile_id, "Profile ID");
  if (allowance_seconds == 0 || allowance_seconds > kMaximumAllowanceSeconds) {
    throw std::invalid_argument("Daily allowance must be between 1 and 86400 seconds");
  }
  Statement statement(impl_->database_, R"sql(
    INSERT INTO daily_policy (profile_id, allowance_milliseconds) VALUES (?, ?)
    ON CONFLICT(profile_id) DO UPDATE SET
      allowance_milliseconds = excluded.allowance_milliseconds
  )sql");
  bind_text(statement.get(), 1, profile_id);
  bind_u64(statement.get(), 2,
           static_cast<std::uint64_t>(allowance_seconds) * kMillisecondsPerSecond);
  step_done(impl_->database_, statement.get());
}

std::optional<std::uint32_t>
DailyTimePolicyStore::find_daily_allowance_seconds(
    const std::string& profile_id) const {
  validate_identifier(profile_id, "Profile ID");
  const auto allowance = impl_->find_allowance(profile_id);
  if (!allowance.has_value()) {
    return std::nullopt;
  }
  return static_cast<std::uint32_t>(*allowance / kMillisecondsPerSecond);
}

void DailyTimePolicyStore::remove_unused_daily_allowance(
    const std::string& profile_id) {
  validate_identifier(profile_id, "Profile ID");
  execute(impl_->database_, "BEGIN IMMEDIATE");
  try {
    Statement in_use(impl_->database_, R"sql(
      SELECT
        EXISTS(SELECT 1 FROM daily_usage WHERE profile_id = ?) OR
        EXISTS(SELECT 1 FROM active_session WHERE profile_id = ?)
    )sql");
    bind_text(in_use.get(), 1, profile_id);
    bind_text(in_use.get(), 2, profile_id);
    if (sqlite3_step(in_use.get()) != SQLITE_ROW) {
      throw std::runtime_error("Could not inspect daily time-policy usage");
    }
    if (sqlite3_column_int(in_use.get(), 0) != 0) {
      throw std::invalid_argument("Daily allowance has usage or an active session");
    }
    Statement clock(impl_->database_,
                    "DELETE FROM profile_clock WHERE profile_id = ?");
    bind_text(clock.get(), 1, profile_id);
    step_done(impl_->database_, clock.get());
    Statement policy(impl_->database_,
                     "DELETE FROM daily_policy WHERE profile_id = ?");
    bind_text(policy.get(), 1, profile_id);
    step_done(impl_->database_, policy.get());
    execute(impl_->database_, "COMMIT");
  } catch (...) {
    execute(impl_->database_, "ROLLBACK");
    throw;
  }
}

DailyTimeStatus DailyTimePolicyStore::status(const std::string& profile_id,
                                             const TimePolicySample& sample) {
  validate_identifier(profile_id, "Profile ID");
  validate_sample(sample);
  const auto daily_allowance = impl_->allowance(profile_id);
  impl_->observe_clock(profile_id, sample);
  return make_status(daily_allowance,
                     impl_->usage(profile_id, sample.local_date).used_milliseconds);
}

DailyTimeDecision DailyTimePolicyStore::begin_session(
    const std::string& profile_id, const std::string& session_id,
    const std::string& item_id, const TimePolicySample& sample) {
  validate_identifier(profile_id, "Profile ID");
  validate_identifier(session_id, "Session ID");
  validate_identifier(item_id, "Item ID");
  validate_sample(sample);
  execute(impl_->database_, "BEGIN IMMEDIATE");
  try {
    const auto daily_allowance = impl_->allowance(profile_id);
    impl_->observe_clock(profile_id, sample);
    const auto current = make_status(
        daily_allowance,
        impl_->usage(profile_id, sample.local_date).used_milliseconds);
    if (current.expired) {
      execute(impl_->database_, "COMMIT");
      return DailyTimeDecision{
          .status = current,
          .notices = {},
          .launch_allowed = false,
          .save_and_exit_required = false,
      };
    }
    if (impl_->active_session().has_value()) {
      throw std::invalid_argument("A time-policy session is already active");
    }
    Statement insert(impl_->database_, R"sql(
      INSERT INTO active_session
        (singleton, profile_id, session_id, item_id, local_date,
         last_monotonic_milliseconds, last_utc_seconds)
      VALUES (1, ?, ?, ?, ?, ?, ?)
    )sql");
    bind_text(insert.get(), 1, profile_id);
    bind_text(insert.get(), 2, session_id);
    bind_text(insert.get(), 3, item_id);
    bind_text(insert.get(), 4, sample.local_date);
    bind_u64(insert.get(), 5, sample.monotonic_milliseconds);
    sqlite3_bind_int64(insert.get(), 6, sample.utc_seconds);
    step_done(impl_->database_, insert.get());
    execute(impl_->database_, "COMMIT");
    return DailyTimeDecision{
        .status = current,
        .notices = {},
        .launch_allowed = true,
        .save_and_exit_required = false,
    };
  } catch (...) {
    execute(impl_->database_, "ROLLBACK");
    throw;
  }
}

DailyTimeDecision DailyTimePolicyStore::update_active_session(
    const std::string& session_id, const TimePolicySample& sample,
    bool keep_active) {
  validate_identifier(session_id, "Session ID");
  validate_sample(sample);
  execute(impl_->database_, "BEGIN IMMEDIATE");
  try {
    const auto active = impl_->active_session();
    if (!active.has_value() || active->session_id != session_id) {
      throw std::invalid_argument("Time-policy session is not active");
    }
    impl_->observe_clock(active->profile_id, sample);
    if (sample.monotonic_milliseconds < active->last_monotonic_milliseconds) {
      throw std::invalid_argument("Monotonic time moved backwards");
    }
    const auto result = impl_->charge(
        *active,
        sample.monotonic_milliseconds - active->last_monotonic_milliseconds,
        keep_active, sample);
    execute(impl_->database_, "COMMIT");
    return result;
  } catch (...) {
    execute(impl_->database_, "ROLLBACK");
    throw;
  }
}

DailyTimeDecision DailyTimePolicyStore::checkpoint_session(
    const std::string& session_id, const TimePolicySample& sample) {
  return update_active_session(session_id, sample, true);
}

DailyTimeDecision DailyTimePolicyStore::pause_session(
    const std::string& session_id, const TimePolicySample& sample) {
  return update_active_session(session_id, sample, false);
}

std::optional<DailyTimeDecision>
DailyTimePolicyStore::recover_interrupted_session(
    const TimePolicySample& sample) {
  validate_sample(sample);
  execute(impl_->database_, "BEGIN IMMEDIATE");
  try {
    const auto active = impl_->active_session();
    if (!active.has_value()) {
      execute(impl_->database_, "COMMIT");
      return std::nullopt;
    }
    impl_->observe_clock(active->profile_id, sample);
    std::uint64_t recovery_charge = kRecoveryCheckpointMilliseconds;
    if (sample.local_date == active->local_date &&
        sample.utc_seconds >= active->last_utc_seconds) {
      const auto wall_delta = static_cast<std::uint64_t>(
          sample.utc_seconds - active->last_utc_seconds);
      recovery_charge =
          std::min(wall_delta, kRecoveryCheckpointMilliseconds /
                                   kMillisecondsPerSecond) *
          kMillisecondsPerSecond;
    }
    const auto result = impl_->charge(*active, recovery_charge, false, sample);
    execute(impl_->database_, "COMMIT");
    return result;
  } catch (...) {
    execute(impl_->database_, "ROLLBACK");
    throw;
  }
}

}  // namespace sprout::launcher
