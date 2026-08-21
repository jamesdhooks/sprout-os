#include "sprout/launcher/game_library_repository.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>
#include <utility>

namespace sprout::launcher {
namespace {

class Statement {
 public:
  Statement(sqlite3* database, std::string_view sql) {
    const std::string text(sql);
    if (sqlite3_prepare_v2(database, text.c_str(), -1, &statement_, nullptr) !=
        SQLITE_OK) {
      const std::string message = sqlite3_errmsg(database);
      sqlite3_finalize(statement_);
      statement_ = nullptr;
      throw std::runtime_error(message);
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
  const std::string text(sql);
  if (sqlite3_exec(database, text.c_str(), nullptr, nullptr, &error) == SQLITE_OK) {
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

void bind_text(sqlite3_stmt* statement, int index, std::string_view value) {
  if (sqlite3_bind_text(statement, index, value.data(),
                        static_cast<int>(value.size()), SQLITE_TRANSIENT) !=
      SQLITE_OK) {
    throw std::runtime_error("Could not bind game-library text value");
  }
}

std::string column_text(sqlite3_stmt* statement, int index) {
  const auto* value = sqlite3_column_text(statement, index);
  return value == nullptr ? std::string{} :
                            std::string(reinterpret_cast<const char*>(value));
}

std::optional<std::int64_t> optional_integer(sqlite3_stmt* statement,
                                             int index) {
  if (sqlite3_column_type(statement, index) == SQLITE_NULL) return std::nullopt;
  return sqlite3_column_int64(statement, index);
}

std::string_view source_id(GameSource source) {
  return source == GameSource::Native ? "native" : "emulated";
}

GameSource source_from_id(std::string_view source) {
  if (source == "native") return GameSource::Native;
  if (source == "emulated") return GameSource::Emulated;
  throw std::runtime_error("Game library contains an unsupported source");
}

std::string_view verdict_id(GameReviewVerdict verdict) {
  return verdict == GameReviewVerdict::Positive ? "positive" : "negative";
}

std::optional<GameReviewVerdict> verdict_from_column(sqlite3_stmt* statement,
                                                     int index) {
  if (sqlite3_column_type(statement, index) == SQLITE_NULL) return std::nullopt;
  const auto value = column_text(statement, index);
  if (value == "positive") return GameReviewVerdict::Positive;
  if (value == "negative") return GameReviewVerdict::Negative;
  throw std::runtime_error("Game library contains an unsupported verdict");
}

std::string_view outcome_id(GameLaunchOutcome outcome) {
  switch (outcome) {
    case GameLaunchOutcome::Active: return "active";
    case GameLaunchOutcome::Completed: return "completed";
    case GameLaunchOutcome::Abnormal: return "abnormal";
    case GameLaunchOutcome::Interrupted: return "interrupted";
  }
  return "abnormal";
}

void require_identity(std::string_view value, std::string_view name) {
  if (value.empty() || value.size() > 256) {
    throw std::invalid_argument(std::string(name) + " is invalid");
  }
}

}  // namespace

std::string_view game_platform_id(GamePlatform platform) noexcept {
  switch (platform) {
    case GamePlatform::GameBoy: return "gb";
    case GamePlatform::GameBoyColor: return "gbc";
    case GamePlatform::GameBoyAdvance: return "gba";
    case GamePlatform::NintendoEntertainmentSystem: return "nes";
    case GamePlatform::SuperNintendo: return "snes";
    case GamePlatform::SegaGenesis: return "genesis";
    case GamePlatform::SegaMasterSystem: return "master-system";
    case GamePlatform::SegaGameGear: return "game-gear";
    case GamePlatform::SegaCD: return "sega-cd";
    case GamePlatform::TurboGrafx16: return "turbografx-16";
    case GamePlatform::NeoGeo: return "neo-geo";
    case GamePlatform::OnionArcade: return "onion-arcade";
    case GamePlatform::PlayStation: return "playstation";
    case GamePlatform::Pico8: return "pico-8";
    case GamePlatform::SproutArcade: return "sprout-arcade";
    case GamePlatform::Unknown: return "unknown";
  }
  return "unknown";
}

std::string_view game_platform_short_label(GamePlatform platform) noexcept {
  switch (platform) {
    case GamePlatform::GameBoy: return "GB";
    case GamePlatform::GameBoyColor: return "GBC";
    case GamePlatform::GameBoyAdvance: return "GBA";
    case GamePlatform::NintendoEntertainmentSystem: return "NES";
    case GamePlatform::SuperNintendo: return "SFC";
    case GamePlatform::SegaGenesis: return "GEN";
    case GamePlatform::SegaMasterSystem: return "SMS";
    case GamePlatform::SegaGameGear: return "GG";
    case GamePlatform::SegaCD: return "SCD";
    case GamePlatform::TurboGrafx16: return "PCE";
    case GamePlatform::NeoGeo: return "NEO";
    case GamePlatform::OnionArcade: return "ARC";
    case GamePlatform::PlayStation: return "PS";
    case GamePlatform::Pico8: return "PICO";
    case GamePlatform::SproutArcade: return "SPROUT";
    case GamePlatform::Unknown: return "?";
  }
  return "?";
}

std::string_view game_platform_name(GamePlatform platform) noexcept {
  switch (platform) {
    case GamePlatform::GameBoy: return "Game Boy";
    case GamePlatform::GameBoyColor: return "Game Boy Color";
    case GamePlatform::GameBoyAdvance: return "Game Boy Advance";
    case GamePlatform::NintendoEntertainmentSystem: return "Nintendo";
    case GamePlatform::SuperNintendo: return "Super Nintendo";
    case GamePlatform::SegaGenesis: return "Sega Genesis";
    case GamePlatform::SegaMasterSystem: return "Master System";
    case GamePlatform::SegaGameGear: return "Game Gear";
    case GamePlatform::SegaCD: return "Sega CD";
    case GamePlatform::TurboGrafx16: return "TurboGrafx-16";
    case GamePlatform::NeoGeo: return "Neo Geo";
    case GamePlatform::OnionArcade: return "Arcade";
    case GamePlatform::PlayStation: return "PlayStation";
    case GamePlatform::Pico8: return "PICO-8";
    case GamePlatform::SproutArcade: return "Sprout Arcade";
    case GamePlatform::Unknown: return "Unknown";
  }
  return "Unknown";
}

GamePlatform game_platform_from_id(std::string_view id) noexcept {
  constexpr std::array<GamePlatform, 15> platforms{
      GamePlatform::GameBoy, GamePlatform::GameBoyColor,
      GamePlatform::GameBoyAdvance, GamePlatform::NintendoEntertainmentSystem,
      GamePlatform::SuperNintendo, GamePlatform::SegaGenesis,
      GamePlatform::SegaMasterSystem, GamePlatform::SegaGameGear,
      GamePlatform::SegaCD, GamePlatform::TurboGrafx16, GamePlatform::NeoGeo,
      GamePlatform::OnionArcade, GamePlatform::PlayStation,
      GamePlatform::Pico8, GamePlatform::SproutArcade};
  const auto found = std::find_if(platforms.begin(), platforms.end(),
                                  [id](GamePlatform platform) {
                                    return game_platform_id(platform) == id;
                                  });
  return found == platforms.end() ? GamePlatform::Unknown : *found;
}

GamePlatform game_platform_from_library_label(std::string_view label,
                                              GameSource source) noexcept {
  if (source == GameSource::Native) return GamePlatform::SproutArcade;
  if (label == "GB") return GamePlatform::GameBoy;
  if (label == "GBC") return GamePlatform::GameBoyColor;
  if (label == "GBA") return GamePlatform::GameBoyAdvance;
  if (label == "NES") return GamePlatform::NintendoEntertainmentSystem;
  if (label == "SFC" || label == "SNES") return GamePlatform::SuperNintendo;
  if (label == "GEN" || label == "MD") return GamePlatform::SegaGenesis;
  if (label == "SMS" || label == "MS") return GamePlatform::SegaMasterSystem;
  if (label == "GG") return GamePlatform::SegaGameGear;
  if (label == "SCD" || label == "SEGACD") return GamePlatform::SegaCD;
  if (label == "PCE") return GamePlatform::TurboGrafx16;
  if (label == "NEO" || label == "NEOGEO") return GamePlatform::NeoGeo;
  if (label == "ARCADE" || label == "ARC") return GamePlatform::OnionArcade;
  if (label == "PS" || label == "PSX") return GamePlatform::PlayStation;
  if (label == "PICO" || label == "PICO-8") return GamePlatform::Pico8;
  return GamePlatform::Unknown;
}

class GameLibraryRepository::Impl {
 public:
  explicit Impl(const std::filesystem::path& database_path) {
    const auto path = path_as_utf8(database_path);
    if (sqlite3_open_v2(path.c_str(), &database_,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                            SQLITE_OPEN_FULLMUTEX,
                        nullptr) != SQLITE_OK) {
      const std::string message = database_ == nullptr
                                      ? "Could not open game library"
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
      throw std::runtime_error("Could not read game-library schema version");
    }
    return static_cast<std::uint32_t>(sqlite3_column_int64(statement.get(), 0));
  }

 private:
  void migrate() {
    const auto version = schema_version();
    if (version > kGameLibraryDatabaseSchemaVersion) {
      throw std::runtime_error("Game-library database is newer than this build");
    }
    if (version == kGameLibraryDatabaseSchemaVersion) return;
    execute(database_, "BEGIN IMMEDIATE");
    try {
      execute(database_, R"sql(
        CREATE TABLE library_items (
          item_id TEXT PRIMARY KEY NOT NULL CHECK(length(item_id) > 0),
          title TEXT NOT NULL CHECK(length(title) > 0),
          platform_id TEXT NOT NULL CHECK(length(platform_id) > 0),
          source TEXT NOT NULL CHECK(source IN ('emulated', 'native')),
          artwork_path TEXT NOT NULL DEFAULT '',
          child_eligible INTEGER NOT NULL CHECK(child_eligible IN (0, 1)),
          available INTEGER NOT NULL CHECK(available IN (0, 1)),
          first_seen_at INTEGER NOT NULL,
          last_seen_at INTEGER NOT NULL
        );
        CREATE TABLE game_screenshots (
          item_id TEXT NOT NULL REFERENCES library_items(item_id) ON DELETE CASCADE,
          position INTEGER NOT NULL CHECK(position >= 0),
          path TEXT NOT NULL CHECK(length(path) > 0),
          PRIMARY KEY(item_id, position)
        );
        CREATE TABLE household_game_state (
          item_id TEXT PRIMARY KEY NOT NULL REFERENCES library_items(item_id) ON DELETE CASCADE,
          recommended INTEGER NOT NULL DEFAULT 0 CHECK(recommended IN (0, 1)),
          for_kids INTEGER NOT NULL DEFAULT 0 CHECK(for_kids IN (0, 1)),
          hidden INTEGER NOT NULL DEFAULT 0 CHECK(hidden IN (0, 1)),
          updated_by TEXT NOT NULL DEFAULT '',
          updated_at INTEGER NOT NULL DEFAULT 0
        );
        CREATE TABLE profile_game_state (
          profile_id TEXT NOT NULL CHECK(length(profile_id) > 0),
          item_id TEXT NOT NULL REFERENCES library_items(item_id) ON DELETE CASCADE,
          favorite INTEGER NOT NULL DEFAULT 0 CHECK(favorite IN (0, 1)),
          child_allowed INTEGER NOT NULL DEFAULT 0 CHECK(child_allowed IN (0, 1)),
          verdict TEXT CHECK(verdict IN ('positive', 'negative')),
          completed INTEGER NOT NULL DEFAULT 0 CHECK(completed IN (0, 1)),
          reviewed_at INTEGER,
          completed_at INTEGER,
          PRIMARY KEY(profile_id, item_id)
        );
        CREATE TABLE play_sessions (
          session_id TEXT PRIMARY KEY NOT NULL CHECK(length(session_id) > 0),
          profile_id TEXT NOT NULL CHECK(length(profile_id) > 0),
          item_id TEXT NOT NULL REFERENCES library_items(item_id) ON DELETE RESTRICT,
          launch_kind TEXT NOT NULL CHECK(length(launch_kind) > 0),
          started_at INTEGER NOT NULL,
          ended_at INTEGER,
          active_milliseconds INTEGER NOT NULL DEFAULT 0 CHECK(active_milliseconds >= 0),
          outcome TEXT NOT NULL CHECK(outcome IN ('active', 'completed', 'abnormal', 'interrupted'))
        );
        CREATE TABLE game_library_metadata (
          key TEXT PRIMARY KEY NOT NULL CHECK(length(key) > 0),
          value TEXT NOT NULL
        );
        CREATE INDEX library_items_platform_index
          ON library_items(available, platform_id, title);
        CREATE INDEX play_sessions_profile_item_index
          ON play_sessions(profile_id, item_id, started_at);
        PRAGMA user_version = 1;
      )sql");
      execute(database_, "COMMIT");
    } catch (...) {
      try { execute(database_, "ROLLBACK"); } catch (...) {}
      throw;
    }
  }

  sqlite3* database_{nullptr};
};

GameLibraryRepository::GameLibraryRepository(
    const std::filesystem::path& database_path)
    : impl_(std::make_unique<Impl>(database_path)) {}
GameLibraryRepository::~GameLibraryRepository() = default;
GameLibraryRepository::GameLibraryRepository(GameLibraryRepository&&) noexcept = default;
GameLibraryRepository& GameLibraryRepository::operator=(GameLibraryRepository&&) noexcept = default;

std::uint32_t GameLibraryRepository::database_schema_version() const {
  return impl_->schema_version();
}

void GameLibraryRepository::reconcile_inventory(
    const std::vector<DiscoveredGame>& games, std::int64_t observed_at) {
  auto* database = impl_->database();
  execute(database, "BEGIN IMMEDIATE");
  try {
    execute(database, "UPDATE library_items SET available = 0");
    Statement upsert(database, R"sql(
      INSERT INTO library_items(item_id, title, platform_id, source,
        artwork_path, child_eligible, available, first_seen_at, last_seen_at)
      VALUES(?, ?, ?, ?, ?, ?, 1, ?, ?)
      ON CONFLICT(item_id) DO UPDATE SET
        title=excluded.title, platform_id=excluded.platform_id,
        source=excluded.source, artwork_path=excluded.artwork_path,
        child_eligible=excluded.child_eligible, available=1,
        last_seen_at=excluded.last_seen_at
    )sql");
    Statement delete_screenshots(
        database, "DELETE FROM game_screenshots WHERE item_id = ?");
    Statement insert_screenshot(database, R"sql(
      INSERT INTO game_screenshots(item_id, position, path) VALUES(?, ?, ?)
    )sql");
    for (const auto& game : games) {
      require_identity(game.item_id, "Game item ID");
      if (game.title.empty()) throw std::invalid_argument("Game title is required");
      sqlite3_reset(upsert.get());
      sqlite3_clear_bindings(upsert.get());
      bind_text(upsert.get(), 1, game.item_id);
      bind_text(upsert.get(), 2, game.title);
      bind_text(upsert.get(), 3, game_platform_id(game.platform));
      bind_text(upsert.get(), 4, source_id(game.source));
      bind_text(upsert.get(), 5, path_as_utf8(game.artwork_path));
      sqlite3_bind_int(upsert.get(), 6, game.child_eligible ? 1 : 0);
      sqlite3_bind_int64(upsert.get(), 7, observed_at);
      sqlite3_bind_int64(upsert.get(), 8, observed_at);
      if (sqlite3_step(upsert.get()) != SQLITE_DONE) {
        throw std::runtime_error(sqlite3_errmsg(database));
      }
      sqlite3_reset(delete_screenshots.get());
      sqlite3_clear_bindings(delete_screenshots.get());
      bind_text(delete_screenshots.get(), 1, game.item_id);
      if (sqlite3_step(delete_screenshots.get()) != SQLITE_DONE) {
        throw std::runtime_error(sqlite3_errmsg(database));
      }
      for (std::size_t index = 0; index < game.screenshot_paths.size(); ++index) {
        if (game.screenshot_paths[index].empty()) continue;
        sqlite3_reset(insert_screenshot.get());
        sqlite3_clear_bindings(insert_screenshot.get());
        bind_text(insert_screenshot.get(), 1, game.item_id);
        sqlite3_bind_int64(insert_screenshot.get(), 2,
                           static_cast<sqlite3_int64>(index));
        bind_text(insert_screenshot.get(), 3,
                  path_as_utf8(game.screenshot_paths[index]));
        if (sqlite3_step(insert_screenshot.get()) != SQLITE_DONE) {
          throw std::runtime_error(sqlite3_errmsg(database));
        }
      }
    }
    execute(database, "COMMIT");
  } catch (...) {
    try { execute(database, "ROLLBACK"); } catch (...) {}
    throw;
  }
}

std::vector<GameLibraryRecord> GameLibraryRepository::list_for_profile(
    const std::string& profile_id, bool include_unavailable) const {
  require_identity(profile_id, "Profile ID");
  auto* database = impl_->database();
  Statement statement(database, R"sql(
    SELECT i.item_id, i.title, i.platform_id, i.source, i.artwork_path,
      i.child_eligible, i.available, i.first_seen_at, i.last_seen_at,
      COALESCE(h.recommended, 0), COALESCE(h.for_kids, 0),
      COALESCE(h.hidden, 0), COALESCE(h.updated_by, ''),
      COALESCE(h.updated_at, 0), COALESCE(p.favorite, 0),
      COALESCE(p.child_allowed, 0), p.verdict, COALESCE(p.completed, 0),
      p.reviewed_at, p.completed_at,
      COUNT(CASE WHEN s.outcome != 'active' THEN 1 END),
      COALESCE(SUM(CASE WHEN s.outcome != 'active' THEN s.active_milliseconds ELSE 0 END), 0),
      MAX(CASE WHEN s.outcome != 'active' THEN s.started_at END)
    FROM library_items i
    LEFT JOIN household_game_state h ON h.item_id = i.item_id
    LEFT JOIN profile_game_state p ON p.item_id = i.item_id AND p.profile_id = ?
    LEFT JOIN play_sessions s ON s.item_id = i.item_id AND s.profile_id = ?
    WHERE (? = 1 OR i.available = 1)
    GROUP BY i.item_id
    ORDER BY lower(i.title), i.item_id
  )sql");
  bind_text(statement.get(), 1, profile_id);
  bind_text(statement.get(), 2, profile_id);
  sqlite3_bind_int(statement.get(), 3, include_unavailable ? 1 : 0);
  std::vector<GameLibraryRecord> records;
  while (sqlite3_step(statement.get()) == SQLITE_ROW) {
    GameLibraryRecord record{
        .inventory = {
            .item_id = column_text(statement.get(), 0),
            .title = column_text(statement.get(), 1),
            .platform = game_platform_from_id(column_text(statement.get(), 2)),
            .source = source_from_id(column_text(statement.get(), 3)),
            .artwork_path = std::filesystem::path(column_text(statement.get(), 4)),
            .screenshot_paths = {},
            .child_eligible = sqlite3_column_int(statement.get(), 5) != 0,
            .available = sqlite3_column_int(statement.get(), 6) != 0,
            .first_seen_at = sqlite3_column_int64(statement.get(), 7),
            .last_seen_at = sqlite3_column_int64(statement.get(), 8),
        },
        .household = {
            .recommended = sqlite3_column_int(statement.get(), 9) != 0,
            .for_kids = sqlite3_column_int(statement.get(), 10) != 0,
            .hidden = sqlite3_column_int(statement.get(), 11) != 0,
            .updated_by = column_text(statement.get(), 12),
            .updated_at = sqlite3_column_int64(statement.get(), 13),
        },
        .profile = {
            .favorite = sqlite3_column_int(statement.get(), 14) != 0,
            .child_allowed = sqlite3_column_int(statement.get(), 15) != 0,
            .verdict = verdict_from_column(statement.get(), 16),
            .completed = sqlite3_column_int(statement.get(), 17) != 0,
            .reviewed_at = optional_integer(statement.get(), 18),
            .completed_at = optional_integer(statement.get(), 19),
        },
        .play = {
            .launch_count = static_cast<std::uint64_t>(sqlite3_column_int64(statement.get(), 20)),
            .active_milliseconds = static_cast<std::uint64_t>(sqlite3_column_int64(statement.get(), 21)),
            .last_played_at = optional_integer(statement.get(), 22),
        },
        .child_allowed_profile_ids = {},
    };
    Statement screenshots(database,
        "SELECT path FROM game_screenshots WHERE item_id = ? ORDER BY position");
    bind_text(screenshots.get(), 1, record.inventory.item_id);
    while (sqlite3_step(screenshots.get()) == SQLITE_ROW) {
      record.inventory.screenshot_paths.emplace_back(column_text(screenshots.get(), 0));
    }
    Statement assignments(database, R"sql(
      SELECT profile_id FROM profile_game_state
      WHERE item_id = ? AND child_allowed = 1 ORDER BY profile_id
    )sql");
    bind_text(assignments.get(), 1, record.inventory.item_id);
    while (sqlite3_step(assignments.get()) == SQLITE_ROW) {
      record.child_allowed_profile_ids.push_back(
          column_text(assignments.get(), 0));
    }
    records.push_back(std::move(record));
  }
  return records;
}

std::optional<GameLibraryRecord> GameLibraryRepository::find_for_profile(
    const std::string& item_id, const std::string& profile_id) const {
  auto records = list_for_profile(profile_id, true);
  const auto found = std::find_if(records.begin(), records.end(),
                                  [&](const auto& record) {
                                    return record.inventory.item_id == item_id;
                                  });
  if (found == records.end()) return std::nullopt;
  return *found;
}

void GameLibraryRepository::set_household_state(
    const std::string& item_id, const HouseholdGameState& state) {
  require_identity(item_id, "Game item ID");
  if (state.hidden && state.recommended) {
    throw std::invalid_argument("A hidden game cannot remain recommended");
  }
  Statement statement(impl_->database(), R"sql(
    INSERT INTO household_game_state(item_id, recommended, for_kids, hidden,
      updated_by, updated_at) VALUES(?, ?, ?, ?, ?, ?)
    ON CONFLICT(item_id) DO UPDATE SET recommended=excluded.recommended,
      for_kids=excluded.for_kids, hidden=excluded.hidden,
      updated_by=excluded.updated_by, updated_at=excluded.updated_at
  )sql");
  bind_text(statement.get(), 1, item_id);
  sqlite3_bind_int(statement.get(), 2, state.recommended ? 1 : 0);
  sqlite3_bind_int(statement.get(), 3, state.for_kids ? 1 : 0);
  sqlite3_bind_int(statement.get(), 4, state.hidden ? 1 : 0);
  bind_text(statement.get(), 5, state.updated_by);
  sqlite3_bind_int64(statement.get(), 6, state.updated_at);
  if (sqlite3_step(statement.get()) != SQLITE_DONE) {
    throw std::runtime_error(sqlite3_errmsg(impl_->database()));
  }
}

namespace {
void upsert_profile_boolean(sqlite3* database, const std::string& profile_id,
                            const std::string& item_id,
                            std::string_view column, bool value) {
  require_identity(profile_id, "Profile ID");
  require_identity(item_id, "Game item ID");
  const std::string sql =
      "INSERT INTO profile_game_state(profile_id, item_id, " +
      std::string(column) + " ) VALUES(?, ?, ?) ON CONFLICT(profile_id, item_id) "
      "DO UPDATE SET " + std::string(column) + "=excluded." +
      std::string(column);
  Statement statement(database, sql);
  bind_text(statement.get(), 1, profile_id);
  bind_text(statement.get(), 2, item_id);
  sqlite3_bind_int(statement.get(), 3, value ? 1 : 0);
  if (sqlite3_step(statement.get()) != SQLITE_DONE) {
    throw std::runtime_error(sqlite3_errmsg(database));
  }
}
}  // namespace

void GameLibraryRepository::set_favorite(const std::string& profile_id,
                                         const std::string& item_id,
                                         bool favorite) {
  upsert_profile_boolean(impl_->database(), profile_id, item_id, "favorite", favorite);
}

void GameLibraryRepository::set_child_allowed(const std::string& profile_id,
                                              const std::string& item_id,
                                              bool allowed) {
  upsert_profile_boolean(impl_->database(), profile_id, item_id,
                         "child_allowed", allowed);
}

void GameLibraryRepository::set_review(
    const std::string& profile_id, const std::string& item_id,
    std::optional<GameReviewVerdict> verdict, std::int64_t reviewed_at) {
  require_identity(profile_id, "Profile ID");
  require_identity(item_id, "Game item ID");
  Statement statement(impl_->database(), R"sql(
    INSERT INTO profile_game_state(profile_id, item_id, verdict, reviewed_at)
    VALUES(?, ?, ?, ?)
    ON CONFLICT(profile_id, item_id) DO UPDATE SET
      verdict=excluded.verdict, reviewed_at=excluded.reviewed_at
  )sql");
  bind_text(statement.get(), 1, profile_id);
  bind_text(statement.get(), 2, item_id);
  if (verdict.has_value()) bind_text(statement.get(), 3, verdict_id(*verdict));
  else sqlite3_bind_null(statement.get(), 3);
  if (verdict.has_value()) sqlite3_bind_int64(statement.get(), 4, reviewed_at);
  else sqlite3_bind_null(statement.get(), 4);
  if (sqlite3_step(statement.get()) != SQLITE_DONE) {
    throw std::runtime_error(sqlite3_errmsg(impl_->database()));
  }
}

void GameLibraryRepository::set_completed(const std::string& profile_id,
                                          const std::string& item_id,
                                          bool completed,
                                          std::int64_t completed_at) {
  require_identity(profile_id, "Profile ID");
  require_identity(item_id, "Game item ID");
  Statement statement(impl_->database(), R"sql(
    INSERT INTO profile_game_state(profile_id, item_id, completed, completed_at)
    VALUES(?, ?, ?, ?)
    ON CONFLICT(profile_id, item_id) DO UPDATE SET
      completed=excluded.completed, completed_at=excluded.completed_at
  )sql");
  bind_text(statement.get(), 1, profile_id);
  bind_text(statement.get(), 2, item_id);
  sqlite3_bind_int(statement.get(), 3, completed ? 1 : 0);
  if (completed) sqlite3_bind_int64(statement.get(), 4, completed_at);
  else sqlite3_bind_null(statement.get(), 4);
  if (sqlite3_step(statement.get()) != SQLITE_DONE) {
    throw std::runtime_error(sqlite3_errmsg(impl_->database()));
  }
}

void GameLibraryRepository::begin_play_session(
    const std::string& session_id, const std::string& profile_id,
    const std::string& item_id, std::string_view launch_kind,
    std::int64_t started_at) {
  require_identity(session_id, "Session ID");
  require_identity(profile_id, "Profile ID");
  require_identity(item_id, "Game item ID");
  require_identity(launch_kind, "Launch kind");
  Statement statement(impl_->database(), R"sql(
    INSERT INTO play_sessions(session_id, profile_id, item_id, launch_kind,
      started_at, outcome) VALUES(?, ?, ?, ?, ?, 'active')
  )sql");
  bind_text(statement.get(), 1, session_id);
  bind_text(statement.get(), 2, profile_id);
  bind_text(statement.get(), 3, item_id);
  bind_text(statement.get(), 4, launch_kind);
  sqlite3_bind_int64(statement.get(), 5, started_at);
  if (sqlite3_step(statement.get()) != SQLITE_DONE) {
    throw std::runtime_error(sqlite3_errmsg(impl_->database()));
  }
}

void GameLibraryRepository::checkpoint_play_session(
    const std::string& session_id, std::uint64_t active_milliseconds) {
  Statement statement(impl_->database(), R"sql(
    UPDATE play_sessions SET active_milliseconds = ?
    WHERE session_id = ? AND outcome = 'active'
  )sql");
  sqlite3_bind_int64(statement.get(), 1,
                     static_cast<sqlite3_int64>(active_milliseconds));
  bind_text(statement.get(), 2, session_id);
  if (sqlite3_step(statement.get()) != SQLITE_DONE ||
      sqlite3_changes(impl_->database()) != 1) {
    throw std::runtime_error("Active play session was not found");
  }
}

void GameLibraryRepository::finish_play_session(
    const std::string& session_id, GameLaunchOutcome outcome,
    std::uint64_t active_milliseconds, std::int64_t ended_at) {
  if (outcome == GameLaunchOutcome::Active) {
    throw std::invalid_argument("Finished play session requires a terminal outcome");
  }
  Statement statement(impl_->database(), R"sql(
    UPDATE play_sessions SET ended_at = ?, active_milliseconds = ?, outcome = ?
    WHERE session_id = ? AND outcome = 'active'
  )sql");
  sqlite3_bind_int64(statement.get(), 1, ended_at);
  sqlite3_bind_int64(statement.get(), 2,
                     static_cast<sqlite3_int64>(active_milliseconds));
  bind_text(statement.get(), 3, outcome_id(outcome));
  bind_text(statement.get(), 4, session_id);
  if (sqlite3_step(statement.get()) != SQLITE_DONE ||
      sqlite3_changes(impl_->database()) != 1) {
    throw std::runtime_error("Active play session was not found");
  }
}

std::size_t GameLibraryRepository::recover_interrupted_sessions(
    std::int64_t ended_at) {
  Statement statement(impl_->database(), R"sql(
    UPDATE play_sessions SET ended_at = ?, outcome = 'interrupted'
    WHERE outcome = 'active'
  )sql");
  sqlite3_bind_int64(statement.get(), 1, ended_at);
  if (sqlite3_step(statement.get()) != SQLITE_DONE) {
    throw std::runtime_error(sqlite3_errmsg(impl_->database()));
  }
  return static_cast<std::size_t>(sqlite3_changes(impl_->database()));
}

bool GameLibraryRepository::metadata_flag(std::string_view key) const {
  Statement statement(impl_->database(),
                      "SELECT value FROM game_library_metadata WHERE key = ?");
  bind_text(statement.get(), 1, key);
  if (sqlite3_step(statement.get()) != SQLITE_ROW) return false;
  return column_text(statement.get(), 0) == "1";
}

void GameLibraryRepository::set_metadata_flag(std::string_view key, bool value) {
  require_identity(key, "Metadata key");
  Statement statement(impl_->database(), R"sql(
    INSERT INTO game_library_metadata(key, value) VALUES(?, ?)
    ON CONFLICT(key) DO UPDATE SET value=excluded.value
  )sql");
  bind_text(statement.get(), 1, key);
  bind_text(statement.get(), 2, value ? "1" : "0");
  if (sqlite3_step(statement.get()) != SQLITE_DONE) {
    throw std::runtime_error(sqlite3_errmsg(impl_->database()));
  }
}

}  // namespace sprout::launcher
