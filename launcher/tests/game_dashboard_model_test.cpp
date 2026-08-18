#include "sprout/launcher/game_dashboard_model.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace sprout::launcher;

void require(bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}

GameLibraryRecord game(std::string id, GamePlatform platform,
                       bool recommended, bool hidden,
                       std::optional<GameReviewVerdict> verdict,
                       std::uint64_t launches, bool completed,
                       std::int64_t last_played = 0) {
  return {
      .inventory = {.item_id = std::move(id),
                    .title = "Game",
                    .platform = platform,
                    .source = platform == GamePlatform::SproutArcade
                                  ? GameSource::Native
                                  : GameSource::Emulated,
                    .child_eligible = true,
                    .available = true},
      .household = {.recommended = recommended,
                    .for_kids = recommended,
                    .hidden = hidden},
      .profile = {.child_allowed = recommended,
                  .verdict = verdict,
                  .completed = completed},
      .play = {.launch_count = launches,
               .active_milliseconds = launches * 1000,
               .last_played_at = launches > 0
                                     ? std::optional<std::int64_t>(last_played)
                                     : std::nullopt},
      .child_allowed_profile_ids = recommended
                                       ? std::vector<std::string>{"child-one"}
                                       : std::vector<std::string>{},
  };
}

std::vector<GameLibraryRecord> records() {
  return {
      game("recommended-new", GamePlatform::GameBoy, true, false,
           std::nullopt, 0, false),
      game("played-unreviewed", GamePlatform::SuperNintendo, false, false,
           std::nullopt, 2, false, 300),
      game("completed-positive", GamePlatform::GameBoy, false, false,
           GameReviewVerdict::Positive, 4, true, 200),
      game("negative", GamePlatform::Pico8, true, false,
           GameReviewVerdict::Negative, 1, false, 100),
      game("hidden", GamePlatform::SuperNintendo, false, true,
           GameReviewVerdict::Positive, 1, false, 50),
  };
}

const DashboardRow* row(const std::vector<DashboardRow>& rows,
                        DashboardRowKind kind) {
  for (const auto& candidate : rows) {
    if (candidate.kind == kind) return &candidate;
  }
  return nullptr;
}

void default_rows_are_flat_and_meaningful() {
  GameDashboardModel model(records());
  const auto rows = model.rows();
  require(row(rows, DashboardRowKind::NextUp) != nullptr,
          "dashboard must start with recommendations");
  require(row(rows, DashboardRowKind::Recent) != nullptr,
          "dashboard must include recent games");
  require(row(rows, DashboardRowKind::Progress) != nullptr,
          "statistics must be an inline row");
  require(row(rows, DashboardRowKind::Platforms) != nullptr,
          "platform collections must be an inline row");
  require(row(rows, DashboardRowKind::Hidden) != nullptr,
          "hidden queue must appear when populated");
  const auto next = row(rows, DashboardRowKind::NextUp);
  require(next->games.front().item_id == "recommended-new" &&
              next->games.front().recommendation_reason ==
                  RecommendationReason::RecommendedUnreviewed,
          "recommended unreviewed games must lead Next Up");
  require(std::none_of(next->games.begin(), next->games.end(),
                       [](const auto& card) {
                         return card.item_id == "negative";
                       }),
          "thumbs-down games must not be recommended again");
}

void statistics_have_exact_denominators() {
  GameDashboardModel model(records());
  const auto stats = model.statistics();
  require(stats.library_total == 4 && stats.library_reviewed == 2,
          "current statistics must exclude hidden games");
  require(stats.recommended_total == 2 && stats.recommended_reviewed == 1,
          "recommended coverage must use the recommended denominator");
  require(stats.outside_recommended_reviewed == 1 && stats.completed == 1,
          "outside-review and completion totals must be independent");
}

void global_filter_changes_all_derived_views() {
  GameDashboardModel model(records());
  GameLibraryFilter filter;
  filter.platforms = {GamePlatform::GameBoy};
  filter.review = ReviewFilter::Unreviewed;
  model.set_filter(filter);
  const auto filtered = model.filtered_games();
  require(filtered.size() == 1 &&
              filtered.front().inventory.item_id == "recommended-new",
          "platform and review filters must combine with AND");
  const auto rows = model.rows();
  require(row(rows, DashboardRowKind::Recent) == nullptr,
          "empty filtered queues must collapse");
  require(row(rows, DashboardRowKind::AllGames)->games.size() == 1,
          "All Games must be the complete filtered result");
  require(model.statistics().library_total == 1,
          "statistics must use the same filter predicate");
  require(model.platform_summaries().size() == 1 &&
              model.platform_summaries().front().platform ==
                  GamePlatform::GameBoy,
          "platform rail must use the same global filter predicate");
  require(model.available_platform_summaries().size() == 3,
          "filter choices must retain every current platform");
  model.toggle_platform(GamePlatform::GameBoy);
  require(model.filter().platforms.empty(),
          "selecting an active platform must toggle it off");
}

void visibility_and_family_filters_are_explicit() {
  GameDashboardModel model(records());
  GameLibraryFilter hidden;
  hidden.visibility = VisibilityFilter::Hidden;
  model.set_filter(hidden);
  require(model.filtered_games().size() == 1 &&
              model.filtered_games().front().inventory.item_id == "hidden",
          "Hidden visibility must not leak current games");

  GameLibraryFilter child;
  child.family = FamilyFilter::Child;
  model.set_filter(child);
  require(model.filtered_games().empty(),
          "child filtering without an identity must fail closed");
  child.child_profile_id = "child-one";
  model.set_filter(child);
  require(model.filtered_games().size() == 2,
          "child filtering must use explicit assignments");
}

void platform_summaries_are_stable_and_review_aware() {
  GameDashboardModel model(records());
  const auto platforms = model.platform_summaries();
  require(platforms.size() == 3,
          "platform row must group current games by stable platform");
  const auto gb = std::find_if(platforms.begin(), platforms.end(),
                               [](const auto& platform) {
                                 return platform.platform == GamePlatform::GameBoy;
                               });
  require(gb != platforms.end() && gb->game_count == 2 &&
              gb->reviewed_count == 1,
          "platform cards must expose count and review progress");
}

}  // namespace

int main() {
  try {
    default_rows_are_flat_and_meaningful();
    statistics_have_exact_denominators();
    global_filter_changes_all_derived_views();
    visibility_and_family_filters_are_explicit();
    platform_summaries_are_stable_and_review_aware();
    std::cout << "game dashboard model tests passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "game dashboard model test failed: " << error.what() << '\n';
    return 1;
  }
}
