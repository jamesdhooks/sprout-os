#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sprout::launcher {

inline constexpr std::uint32_t kGameLibraryDatabaseSchemaVersion = 1;

enum class GamePlatform {
  GameBoy,
  GameBoyColor,
  GameBoyAdvance,
  NintendoEntertainmentSystem,
  SuperNintendo,
  SegaGenesis,
  SegaMasterSystem,
  SegaGameGear,
  SegaCD,
  TurboGrafx16,
  NeoGeo,
  OnionArcade,
  PlayStation,
  Pico8,
  SproutArcade,
  Unknown,
};

enum class GameSource {
  Emulated,
  Native,
};

enum class GameReviewVerdict {
  Positive,
  Negative,
};

enum class GameLaunchOutcome {
  Active,
  Completed,
  Abnormal,
  Interrupted,
};

[[nodiscard]] std::string_view game_platform_id(GamePlatform platform) noexcept;
[[nodiscard]] std::string_view game_platform_short_label(
    GamePlatform platform) noexcept;
[[nodiscard]] std::string_view game_platform_name(GamePlatform platform) noexcept;
[[nodiscard]] GamePlatform game_platform_from_id(std::string_view id) noexcept;
[[nodiscard]] GamePlatform game_platform_from_library_label(
    std::string_view label, GameSource source) noexcept;

struct DiscoveredGame {
  std::string item_id;
  std::string title;
  GamePlatform platform{GamePlatform::Unknown};
  GameSource source{GameSource::Emulated};
  std::filesystem::path artwork_path;
  std::vector<std::filesystem::path> screenshot_paths;
  bool child_eligible{false};
};

struct GameInventoryRecord {
  std::string item_id;
  std::string title;
  GamePlatform platform{GamePlatform::Unknown};
  GameSource source{GameSource::Emulated};
  std::filesystem::path artwork_path;
  std::vector<std::filesystem::path> screenshot_paths;
  bool child_eligible{false};
  bool available{false};
  std::int64_t first_seen_at{0};
  std::int64_t last_seen_at{0};
};

struct HouseholdGameState {
  bool recommended{false};
  bool for_kids{false};
  bool hidden{false};
  std::string updated_by;
  std::int64_t updated_at{0};
};

struct ProfileGameState {
  bool favorite{false};
  bool child_allowed{false};
  std::optional<GameReviewVerdict> verdict;
  bool completed{false};
  std::optional<std::int64_t> reviewed_at;
  std::optional<std::int64_t> completed_at;
};

struct GamePlaySummary {
  std::uint64_t launch_count{0};
  std::uint64_t active_milliseconds{0};
  std::optional<std::int64_t> last_played_at;
};

struct GameLibraryRecord {
  GameInventoryRecord inventory;
  HouseholdGameState household;
  ProfileGameState profile;
  GamePlaySummary play;
  std::vector<std::string> child_allowed_profile_ids;
};

class GameLibraryRepository {
 public:
  explicit GameLibraryRepository(const std::filesystem::path& database_path);
  ~GameLibraryRepository();

  GameLibraryRepository(const GameLibraryRepository&) = delete;
  GameLibraryRepository& operator=(const GameLibraryRepository&) = delete;
  GameLibraryRepository(GameLibraryRepository&&) noexcept;
  GameLibraryRepository& operator=(GameLibraryRepository&&) noexcept;

  [[nodiscard]] std::uint32_t database_schema_version() const;

  void reconcile_inventory(const std::vector<DiscoveredGame>& games,
                           std::int64_t observed_at);
  [[nodiscard]] std::vector<GameLibraryRecord> list_for_profile(
      const std::string& profile_id, bool include_unavailable = true) const;
  [[nodiscard]] std::optional<GameLibraryRecord> find_for_profile(
      const std::string& item_id, const std::string& profile_id) const;

  void set_household_state(const std::string& item_id,
                           const HouseholdGameState& state);
  void set_favorite(const std::string& profile_id, const std::string& item_id,
                    bool favorite);
  void set_child_allowed(const std::string& profile_id,
                         const std::string& item_id, bool allowed);
  void set_review(const std::string& profile_id, const std::string& item_id,
                  std::optional<GameReviewVerdict> verdict,
                  std::int64_t reviewed_at);
  void set_completed(const std::string& profile_id,
                     const std::string& item_id, bool completed,
                     std::int64_t completed_at);

  void begin_play_session(const std::string& session_id,
                          const std::string& profile_id,
                          const std::string& item_id,
                          std::string_view launch_kind,
                          std::int64_t started_at);
  void checkpoint_play_session(const std::string& session_id,
                               std::uint64_t active_milliseconds);
  void finish_play_session(const std::string& session_id,
                           GameLaunchOutcome outcome,
                           std::uint64_t active_milliseconds,
                           std::int64_t ended_at);
  std::size_t recover_interrupted_sessions(std::int64_t ended_at);

  [[nodiscard]] bool metadata_flag(std::string_view key) const;
  void set_metadata_flag(std::string_view key, bool value);

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace sprout::launcher
