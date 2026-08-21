#include "sprout/launcher/game_dashboard_model.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <tuple>
#include <utility>

namespace sprout::launcher {
namespace {

bool contains_platform(const std::vector<GamePlatform>& platforms,
                       GamePlatform platform) {
  return std::find(platforms.begin(), platforms.end(), platform) !=
         platforms.end();
}

std::int64_t last_played_or_zero(const GameLibraryRecord& game) {
  return game.play.last_played_at.value_or(0);
}

std::vector<DashboardGameCard> cards(
    std::vector<const GameLibraryRecord*> games) {
  std::vector<DashboardGameCard> result;
  result.reserve(games.size());
  for (const auto* game : games) {
    DashboardGameCard card;
    card.item_id = game->inventory.item_id;
    result.push_back(std::move(card));
  }
  return result;
}

}  // namespace

bool GameLibraryFilter::active() const noexcept {
  return !platforms.empty() || review != ReviewFilter::All ||
         progress != ProgressFilter::All || family != FamilyFilter::All ||
         visibility != VisibilityFilter::Current;
}

GameDashboardModel::GameDashboardModel(std::vector<GameLibraryRecord> records)
    : records_(std::move(records)) {}

const GameLibraryFilter& GameDashboardModel::filter() const noexcept {
  return filter_;
}

void GameDashboardModel::set_filter(GameLibraryFilter filter) {
  std::sort(filter.platforms.begin(), filter.platforms.end(),
            [](GamePlatform left, GamePlatform right) {
              return game_platform_id(left) < game_platform_id(right);
            });
  filter.platforms.erase(
      std::unique(filter.platforms.begin(), filter.platforms.end()),
      filter.platforms.end());
  if (filter.family != FamilyFilter::Child) {
    filter.child_profile_id.reset();
  }
  filter_ = std::move(filter);
}

void GameDashboardModel::clear_filter() noexcept { filter_ = {}; }

void GameDashboardModel::toggle_platform(GamePlatform platform) {
  const auto found = std::find(filter_.platforms.begin(),
                               filter_.platforms.end(), platform);
  if (found == filter_.platforms.end()) {
    filter_.platforms.push_back(platform);
  } else {
    filter_.platforms.erase(found);
  }
}

bool GameDashboardModel::matches(const GameLibraryRecord& game,
                                 bool ignore_visibility) const {
  if (!game.inventory.available) return false;
  if (!ignore_visibility) {
    if (filter_.visibility == VisibilityFilter::Current &&
        game.household.hidden) return false;
    if (filter_.visibility == VisibilityFilter::Hidden &&
        !game.household.hidden) return false;
  }
  if (!filter_.platforms.empty() &&
      !contains_platform(filter_.platforms, game.inventory.platform)) return false;
  switch (filter_.review) {
    case ReviewFilter::All: break;
    case ReviewFilter::Unreviewed:
      if (game.profile.verdict.has_value()) return false;
      break;
    case ReviewFilter::Positive:
      if (game.profile.verdict != GameReviewVerdict::Positive) return false;
      break;
    case ReviewFilter::Negative:
      if (game.profile.verdict != GameReviewVerdict::Negative) return false;
      break;
  }
  switch (filter_.progress) {
    case ProgressFilter::All: break;
    case ProgressFilter::Started:
      if (game.play.launch_count == 0 || game.profile.completed) return false;
      break;
    case ProgressFilter::Completed:
      if (!game.profile.completed) return false;
      break;
  }
  switch (filter_.family) {
    case FamilyFilter::All: break;
    case FamilyFilter::Recommended:
      if (!game.household.recommended) return false;
      break;
    case FamilyFilter::ForKids:
      if (!game.household.for_kids) return false;
      break;
    case FamilyFilter::Child:
      if (!filter_.child_profile_id.has_value() ||
          std::find(game.child_allowed_profile_ids.begin(),
                    game.child_allowed_profile_ids.end(),
                    *filter_.child_profile_id) ==
              game.child_allowed_profile_ids.end()) return false;
      break;
  }
  return true;
}

std::vector<GameLibraryRecord> GameDashboardModel::filtered_games() const {
  std::vector<GameLibraryRecord> result;
  std::copy_if(records_.begin(), records_.end(), std::back_inserter(result),
               [this](const auto& game) { return matches(game); });
  return result;
}

GameReviewStatistics GameDashboardModel::statistics() const {
  GameReviewStatistics result;
  for (const auto& game : records_) {
    if (!matches(game)) continue;
    ++result.library_total;
    const bool reviewed = game.profile.verdict.has_value();
    if (reviewed) ++result.library_reviewed;
    if (game.household.recommended) {
      ++result.recommended_total;
      if (reviewed) ++result.recommended_reviewed;
    } else if (reviewed) {
      ++result.outside_recommended_reviewed;
    }
    if (game.profile.completed) ++result.completed;
  }
  return result;
}

std::vector<PlatformSummary> GameDashboardModel::platform_summaries() const {
  std::map<std::string_view, PlatformSummary> summaries;
  for (const auto& game : records_) {
    if (!matches(game)) continue;
    auto& summary = summaries[game_platform_id(game.inventory.platform)];
    summary.platform = game.inventory.platform;
    ++summary.game_count;
    if (game.profile.verdict.has_value()) ++summary.reviewed_count;
  }
  std::vector<PlatformSummary> result;
  for (const auto& [id, summary] : summaries) {
    (void)id;
    result.push_back(summary);
  }
  return result;
}

std::vector<PlatformSummary>
GameDashboardModel::available_platform_summaries() const {
  std::map<std::string_view, PlatformSummary> summaries;
  for (const auto& game : records_) {
    if (!game.inventory.available || game.household.hidden) continue;
    auto& summary = summaries[game_platform_id(game.inventory.platform)];
    summary.platform = game.inventory.platform;
    ++summary.game_count;
    if (game.profile.verdict.has_value()) ++summary.reviewed_count;
  }
  std::vector<PlatformSummary> result;
  for (const auto& [id, summary] : summaries) {
    (void)id;
    result.push_back(summary);
  }
  return result;
}

std::vector<DashboardGameCard> GameDashboardModel::next_up() const {
  struct Candidate {
    const GameLibraryRecord* game;
    RecommendationReason reason;
    int priority;
  };
  std::vector<Candidate> candidates;
  for (const auto& game : records_) {
    if (!matches(game)) continue;
    if (game.profile.verdict == GameReviewVerdict::Negative) continue;
    if (game.household.recommended && !game.profile.verdict.has_value()) {
      candidates.push_back({&game, RecommendationReason::RecommendedUnreviewed, 0});
    } else if (game.play.launch_count > 0 &&
               !game.profile.verdict.has_value()) {
      candidates.push_back({&game, RecommendationReason::PlayedUnreviewed, 1});
    } else if (game.play.launch_count > 0 && !game.profile.completed) {
      candidates.push_back({&game, RecommendationReason::StartedIncomplete, 2});
    } else if (game.play.launch_count == 0) {
      candidates.push_back({&game, RecommendationReason::NeverOpened, 3});
    }
  }
  std::stable_sort(candidates.begin(), candidates.end(),
                   [](const Candidate& left, const Candidate& right) {
                     if (left.priority != right.priority)
                       return left.priority < right.priority;
                     if (left.reason == RecommendationReason::PlayedUnreviewed ||
                         left.reason == RecommendationReason::StartedIncomplete) {
                       return last_played_or_zero(*left.game) >
                              last_played_or_zero(*right.game);
                     }
                     return std::tie(left.game->inventory.title,
                                     left.game->inventory.item_id) <
                            std::tie(right.game->inventory.title,
                                     right.game->inventory.item_id);
                   });

  // Prefer a different platform from the previous card when doing so does not
  // cross a recommendation-priority boundary.
  for (std::size_t index = 1; index < candidates.size(); ++index) {
    if (candidates[index - 1].game->inventory.platform !=
        candidates[index].game->inventory.platform) continue;
    const auto alternative = std::find_if(
        candidates.begin() + static_cast<std::ptrdiff_t>(index + 1),
        candidates.end(), [&](const Candidate& candidate) {
          return candidate.priority == candidates[index].priority &&
                 candidate.game->inventory.platform !=
                     candidates[index - 1].game->inventory.platform;
        });
    if (alternative != candidates.end()) std::iter_swap(candidates.begin() + index, alternative);
  }
  if (candidates.size() > 12) candidates.resize(12);
  std::vector<DashboardGameCard> result;
  for (const auto& candidate : candidates) {
    DashboardGameCard card;
    card.item_id = candidate.game->inventory.item_id;
    card.recommendation_reason = candidate.reason;
    result.push_back(std::move(card));
  }
  return result;
}

std::vector<DashboardRow> GameDashboardModel::rows() const {
  std::vector<DashboardRow> result;
  const auto append_games = [&](DashboardRowKind kind,
                                std::vector<const GameLibraryRecord*> games,
                                bool include_empty = false) {
    if (games.empty() && !include_empty) return;
    DashboardRow row;
    row.kind = kind;
    row.games = cards(std::move(games));
    result.push_back(std::move(row));
  };

  // The dashboard stays deliberately flat: resume what was already in play,
  // then consider the intelligent queue, then browse the complete library.
  // Avoid adding short-lived workflow rows here; those are recommendations,
  // not destinations.
  std::vector<const GameLibraryRecord*> recent;
  for (const auto& game : records_) {
    if (matches(game) && game.play.launch_count > 0) recent.push_back(&game);
  }
  std::stable_sort(recent.begin(), recent.end(), [](const auto* left, const auto* right) {
    return last_played_or_zero(*left) > last_played_or_zero(*right);
  });
  append_games(DashboardRowKind::Recent, std::move(recent));

  const auto recommendations = next_up();
  if (!recommendations.empty()) {
    DashboardRow row;
    row.kind = DashboardRowKind::Recommended;
    row.games = recommendations;
    result.push_back(std::move(row));
  }

  std::vector<const GameLibraryRecord*> all;
  for (const auto& game : records_) {
    if (matches(game)) all.push_back(&game);
  }
  append_games(DashboardRowKind::AllGames, std::move(all), true);

  // Platforms are filter controls, so their availability never shrinks to
  // the currently selected platform(s). An active card is styled by the view.
  const auto platforms = available_platform_summaries();
  if (!platforms.empty()) {
    DashboardRow platform_row;
    platform_row.kind = DashboardRowKind::Platforms;
    platform_row.platforms = platforms;
    result.push_back(std::move(platform_row));
  }

  DashboardRow progress_row;
  progress_row.kind = DashboardRowKind::Progress;
  progress_row.statistics = statistics();
  result.push_back(std::move(progress_row));
  return result;
}

}  // namespace sprout::launcher
