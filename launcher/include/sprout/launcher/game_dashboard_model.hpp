#pragma once

#include "sprout/launcher/game_library_repository.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace sprout::launcher {

enum class ReviewFilter { All, Unreviewed, Positive, Negative };
enum class ProgressFilter { All, Started, Completed };
enum class FamilyFilter { All, Recommended, ForKids, Child };
enum class VisibilityFilter { Current, Hidden };

struct GameLibraryFilter {
  std::vector<GamePlatform> platforms;
  ReviewFilter review{ReviewFilter::All};
  ProgressFilter progress{ProgressFilter::All};
  FamilyFilter family{FamilyFilter::All};
  std::optional<std::string> child_profile_id;
  VisibilityFilter visibility{VisibilityFilter::Current};

  [[nodiscard]] bool active() const noexcept;
};

enum class RecommendationReason {
  RecommendedUnreviewed,
  PlayedUnreviewed,
  StartedIncomplete,
  NeverOpened,
};

enum class DashboardRowKind {
  NextUp,
  Recent,
  Progress,
  Platforms,
  NeedsReview,
  Unplayed,
  FinishNext,
  Recommended,
  AllGames,
  Hidden,
  Settings,
};

struct DashboardGameCard {
  std::string item_id;
  std::optional<RecommendationReason> recommendation_reason;
};

struct GameReviewStatistics {
  std::uint64_t recommended_total{0};
  std::uint64_t recommended_reviewed{0};
  std::uint64_t library_total{0};
  std::uint64_t library_reviewed{0};
  std::uint64_t outside_recommended_reviewed{0};
  std::uint64_t completed{0};
};

struct PlatformSummary {
  GamePlatform platform{GamePlatform::Unknown};
  std::uint64_t game_count{0};
  std::uint64_t reviewed_count{0};
};

struct DashboardRow {
  DashboardRowKind kind{DashboardRowKind::AllGames};
  std::vector<DashboardGameCard> games;
  std::vector<PlatformSummary> platforms;
  std::vector<std::string> settings;
  std::optional<GameReviewStatistics> statistics;
};

class GameDashboardModel {
 public:
  explicit GameDashboardModel(std::vector<GameLibraryRecord> records);

  [[nodiscard]] const GameLibraryFilter& filter() const noexcept;
  void set_filter(GameLibraryFilter filter);
  void clear_filter() noexcept;
  void toggle_platform(GamePlatform platform);

  [[nodiscard]] std::vector<GameLibraryRecord> filtered_games() const;
  [[nodiscard]] GameReviewStatistics statistics() const;
  [[nodiscard]] std::vector<PlatformSummary> platform_summaries() const;
  [[nodiscard]] std::vector<PlatformSummary> available_platform_summaries() const;
  [[nodiscard]] std::vector<DashboardRow> rows() const;

 private:
  [[nodiscard]] bool matches(const GameLibraryRecord& game,
                             bool ignore_visibility = false) const;
  [[nodiscard]] std::vector<DashboardGameCard> next_up() const;

  std::vector<GameLibraryRecord> records_;
  GameLibraryFilter filter_;
};

}  // namespace sprout::launcher
