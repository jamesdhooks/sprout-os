#include "sprout/launcher/game_dashboard_presentation.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <utility>

namespace sprout::launcher {
namespace {

constexpr std::array<GameFilterCategory, 5> kFilterCategories{
    GameFilterCategory::Platform, GameFilterCategory::Review,
    GameFilterCategory::Progress, GameFilterCategory::Family,
    GameFilterCategory::Visibility};

std::size_t category_offset(GameFilterCategory category) {
  return static_cast<std::size_t>(category);
}

}  // namespace

GameDashboardPresentation::GameDashboardPresentation(
    GameLibraryRepository& repository, std::string profile_id,
    std::vector<Profile> child_profiles, bool parent_mode, bool big_mode,
    std::string avatar_ref, std::string background_ref, std::string interface_theme,
    std::uint32_t accent_rgb, bool rounded_tiles, bool motion_enabled)
    : repository_(repository),
      profile_id_(std::move(profile_id)),
      child_profiles_(std::move(child_profiles)),
      parent_mode_(parent_mode),
      big_mode_(big_mode),
      profile_avatar_ref_(std::move(avatar_ref)),
      profile_background_ref_(std::move(background_ref)),
      interface_theme_(std::move(interface_theme)),
      accent_rgb_(accent_rgb),
      rounded_tiles_(rounded_tiles),
      motion_enabled_(motion_enabled),
      records_(repository_.list_for_profile(profile_id_, false)),
      model_(records_) {
  if (!parent_mode_) {
    records_.erase(std::remove_if(records_.begin(), records_.end(),
                                  [this](const auto& game) {
      return !game.inventory.child_eligible || game.household.hidden ||
             !game.profile.child_allowed;
    }), records_.end());
    model_ = GameDashboardModel(records_);
  }
  rebuild();
}

GameDashboardStage GameDashboardPresentation::stage() const noexcept {
  return stage_;
}

ReadOnlyView<DashboardRow> GameDashboardPresentation::rows() const noexcept {
  return rows_;
}

std::size_t GameDashboardPresentation::row_focus() const noexcept {
  return row_focus_;
}

std::size_t GameDashboardPresentation::item_focus(
    DashboardRowKind kind) const noexcept {
  return item_focus_[row_offset(kind)];
}

const GameLibraryFilter& GameDashboardPresentation::filter() const noexcept {
  return model_.filter();
}

const GameLibraryFilter& GameDashboardPresentation::draft_filter() const noexcept {
  return draft_filter_;
}

GameFilterCategory GameDashboardPresentation::filter_category() const noexcept {
  return filter_category_;
}

std::size_t GameDashboardPresentation::filter_option_focus() const noexcept {
  return filter_option_focus_;
}

ReadOnlyView<PlatformSummary>
GameDashboardPresentation::available_platforms() const noexcept {
  return available_platforms_;
}

ReadOnlyView<Profile> GameDashboardPresentation::child_profiles() const noexcept {
  return child_profiles_;
}

GameDetailPage GameDashboardPresentation::detail_page() const noexcept {
  return detail_page_;
}

std::size_t GameDashboardPresentation::detail_focus() const noexcept {
  return detail_focus_;
}

std::size_t GameDashboardPresentation::detail_artwork_focus() const noexcept {
  return detail_artwork_focus_;
}

const GameLibraryRecord* GameDashboardPresentation::selected_game() const noexcept {
  if (!detail_item_id_.has_value()) return nullptr;
  const auto found = std::find_if(records_.begin(), records_.end(), [&](const auto& game) {
    return game.inventory.item_id == *detail_item_id_;
  });
  return found == records_.end() ? nullptr : &*found;
}

const GameLibraryRecord* GameDashboardPresentation::find_game(
    std::string_view item_id) const noexcept {
  const auto found = std::find_if(records_.begin(), records_.end(),
                                  [item_id](const auto& game) {
                                    return game.inventory.item_id == item_id;
                                  });
  return found == records_.end() ? nullptr : &*found;
}

std::string_view GameDashboardPresentation::notice() const noexcept {
  return notice_;
}

std::string_view GameDashboardPresentation::search_query() const noexcept { return search_query_; }
ReadOnlyView<GameLibraryRecord> GameDashboardPresentation::search_results() const noexcept { return search_results_; }
std::size_t GameDashboardPresentation::search_keyboard_focus() const noexcept { return search_keyboard_focus_; }
std::size_t GameDashboardPresentation::search_result_focus() const noexcept { return search_result_focus_; }
bool GameDashboardPresentation::search_results_focused() const noexcept { return search_results_focused_; }

bool GameDashboardPresentation::big_mode() const noexcept { return big_mode_; }

std::string_view GameDashboardPresentation::profile_avatar_ref() const noexcept {
  return profile_avatar_ref_;
}

std::string_view GameDashboardPresentation::profile_background_ref() const noexcept {
  return profile_background_ref_;
}

std::string_view GameDashboardPresentation::interface_theme() const noexcept {
  return interface_theme_;
}

std::uint32_t GameDashboardPresentation::accent_rgb() const noexcept {
  return accent_rgb_;
}
bool GameDashboardPresentation::rounded_tiles() const noexcept { return rounded_tiles_; }
bool GameDashboardPresentation::motion_enabled() const noexcept { return motion_enabled_; }

void GameDashboardPresentation::set_big_mode(bool enabled) noexcept {
  big_mode_ = enabled;
}

void GameDashboardPresentation::set_profile_avatar_ref(std::string reference) {
  profile_avatar_ref_ = std::move(reference);
}

void GameDashboardPresentation::set_profile_background_ref(std::string reference) {
  profile_background_ref_ = std::move(reference);
}

void GameDashboardPresentation::set_interface_theme(std::string theme) {
  interface_theme_ = std::move(theme);
}

void GameDashboardPresentation::set_accent_rgb(std::uint32_t accent_rgb) noexcept {
  accent_rgb_ = accent_rgb;
}
void GameDashboardPresentation::set_rounded_tiles(bool rounded) noexcept { rounded_tiles_ = rounded; }
void GameDashboardPresentation::set_motion_enabled(bool enabled) noexcept { motion_enabled_ = enabled; }

void GameDashboardPresentation::open_search() {
  stage_ = GameDashboardStage::Search;
  search_keyboard_focus_ = 0;
  search_result_focus_ = 0;
  search_results_focused_ = false;
  rebuild_search();
}

void GameDashboardPresentation::rebuild_search() {
  search_results_ = model_.filtered_games();
  std::string needle = search_query_;
  std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  search_results_.erase(std::remove_if(search_results_.begin(), search_results_.end(),
      [&needle](const auto& game) {
        std::string title = game.inventory.title;
        std::transform(title.begin(), title.end(), title.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return title.find(needle) == std::string::npos;
      }), search_results_.end());
  if (search_result_focus_ >= search_results_.size()) search_result_focus_ = search_results_.empty() ? 0 : search_results_.size() - 1;
}

void GameDashboardPresentation::handle_search_action(Action action, std::int64_t now) {
  constexpr std::string_view keys = "QWERTYUIOPASDFGHJKLZXCVBNM<_";
  constexpr std::size_t columns = 10;
  if (action == Action::Back) { stage_ = GameDashboardStage::Dashboard; return; }
  if (action == Action::Menu && !search_results_.empty()) {
    detail_item_id_ = search_results_[search_result_focus_].inventory.item_id;
    detail_page_ = GameDetailPage::Overview; detail_focus_ = 0; detail_artwork_focus_ = 0;
    stage_ = GameDashboardStage::Details; return;
  }
  if (search_results_focused_) {
    if (action == Action::Up) { search_results_focused_ = false; return; }
    if (action == Action::Down && !search_results_.empty()) { search_result_focus_ = (search_result_focus_ + 1) % search_results_.size(); return; }
    if (action == Action::Confirm && !search_results_.empty()) {
      detail_item_id_ = search_results_[search_result_focus_].inventory.item_id;
      notice_ = ""; return;
    }
    return;
  }
  if (action == Action::Left) search_keyboard_focus_ = (search_keyboard_focus_ + keys.size() - 1) % keys.size();
  else if (action == Action::Right) search_keyboard_focus_ = (search_keyboard_focus_ + 1) % keys.size();
  else if (action == Action::Up) search_keyboard_focus_ = (search_keyboard_focus_ + keys.size() - columns) % keys.size();
  else if (action == Action::Down) {
    if (search_keyboard_focus_ + columns >= keys.size() && !search_results_.empty()) search_results_focused_ = true;
    else search_keyboard_focus_ = (search_keyboard_focus_ + columns) % keys.size();
  } else if (action == Action::Confirm) {
    const char key = keys[search_keyboard_focus_];
    if (key == '<') { if (!search_query_.empty()) search_query_.pop_back(); }
    else if (key == '_') search_query_.push_back(' ');
    else search_query_.push_back(key);
    rebuild_search();
  }
  (void)now;
}

std::size_t GameDashboardPresentation::row_offset(DashboardRowKind kind) noexcept {
  return static_cast<std::size_t>(kind);
}

std::size_t GameDashboardPresentation::active_item_count() const noexcept {
  if (rows_.empty() || row_focus_ >= rows_.size()) return 0;
  const auto& row = rows_[row_focus_];
  if (!row.games.empty()) return row.games.size();
  if (!row.platforms.empty()) return row.platforms.size();
  if (!row.settings.empty()) return row.settings.size();
  if (row.statistics.has_value()) return 4;
  return 0;
}

std::optional<std::string> GameDashboardPresentation::focused_game_id() const {
  if (rows_.empty() || row_focus_ >= rows_.size()) return std::nullopt;
  const auto& row = rows_[row_focus_];
  if (row.games.empty()) return std::nullopt;
  const auto focus = std::min(item_focus(row.kind), row.games.size() - 1);
  return row.games[focus].item_id;
}

void GameDashboardPresentation::move_row(int delta) {
  if (rows_.empty()) return;
  const auto next = static_cast<long long>(row_focus_) + delta;
  row_focus_ = static_cast<std::size_t>(std::clamp<long long>(
      next, 0, static_cast<long long>(rows_.size() - 1)));
}

void GameDashboardPresentation::move_item(int delta) {
  const auto count = active_item_count();
  if (count == 0 || rows_.empty()) return;
  auto& focus = item_focus_[row_offset(rows_[row_focus_].kind)];
  const auto next = static_cast<long long>(focus) + delta;
  focus = static_cast<std::size_t>(std::clamp<long long>(
      next, 0, static_cast<long long>(count - 1)));
}

void GameDashboardPresentation::open_filters() {
  draft_filter_ = model_.filter();
  filter_category_ = GameFilterCategory::Platform;
  filter_option_focus_ = 0;
  stage_ = GameDashboardStage::Filters;
}

void GameDashboardPresentation::handle_filter_action(Action action) {
  if (action == Action::Back) {
    stage_ = GameDashboardStage::Dashboard;
    return;
  }
  if (action == Action::Left || action == Action::Right) {
    const int delta = action == Action::Left ? -1 : 1;
    const auto count = static_cast<long long>(kFilterCategories.size());
    const auto current = static_cast<long long>(category_offset(filter_category_));
    filter_category_ = kFilterCategories[static_cast<std::size_t>(
        (current + delta + count) % count)];
    filter_option_focus_ = 0;
    return;
  }
  std::size_t option_count = 1;
  switch (filter_category_) {
    case GameFilterCategory::Platform: option_count = available_platforms_.size() + 1; break;
    case GameFilterCategory::Review: option_count = 4; break;
    case GameFilterCategory::Progress: option_count = 3; break;
    case GameFilterCategory::Family: option_count = 3 + child_profiles_.size(); break;
    case GameFilterCategory::Visibility: option_count = 2; break;
  }
  if (action == Action::Up || action == Action::Down) {
    const int delta = action == Action::Up ? -1 : 1;
    filter_option_focus_ = static_cast<std::size_t>(
        (static_cast<long long>(filter_option_focus_) + delta +
         static_cast<long long>(option_count)) %
        static_cast<long long>(option_count));
    return;
  }
  if (action != Action::Confirm) return;
  switch (filter_category_) {
    case GameFilterCategory::Platform:
      if (filter_option_focus_ == 0) draft_filter_.platforms.clear();
      else {
        const auto platform = available_platforms_[filter_option_focus_ - 1].platform;
        const auto found = std::find(draft_filter_.platforms.begin(),
                                     draft_filter_.platforms.end(), platform);
        if (found == draft_filter_.platforms.end()) draft_filter_.platforms.push_back(platform);
        else draft_filter_.platforms.erase(found);
      }
      break;
    case GameFilterCategory::Review:
      draft_filter_.review = static_cast<ReviewFilter>(filter_option_focus_);
      break;
    case GameFilterCategory::Progress:
      draft_filter_.progress = static_cast<ProgressFilter>(filter_option_focus_);
      break;
    case GameFilterCategory::Family:
      if (filter_option_focus_ == 0) draft_filter_.family = FamilyFilter::All;
      else if (filter_option_focus_ == 1) draft_filter_.family = FamilyFilter::Recommended;
      else if (filter_option_focus_ == 2) draft_filter_.family = FamilyFilter::ForKids;
      else {
        draft_filter_.family = FamilyFilter::Child;
        draft_filter_.child_profile_id = child_profiles_[filter_option_focus_ - 3].id;
      }
      break;
    case GameFilterCategory::Visibility:
      draft_filter_.visibility = filter_option_focus_ == 0
                                     ? VisibilityFilter::Current
                                     : VisibilityFilter::Hidden;
      break;
  }
  // Selection is live: each A press immediately updates the dashboard behind
  // this simple picker; B is the only way back out.
  model_.set_filter(draft_filter_);
  rebuild();
}

void GameDashboardPresentation::toggle_review(GameReviewVerdict verdict,
                                              std::int64_t now) {
  std::optional<std::string> item_id = detail_item_id_;
  if (!item_id.has_value()) item_id = focused_game_id();
  if (!item_id.has_value()) return;
  const auto existing = repository_.find_for_profile(*item_id, profile_id_);
  if (!existing.has_value()) return;
  const auto next = existing->profile.verdict == verdict
                        ? std::optional<GameReviewVerdict>{}
                        : std::optional<GameReviewVerdict>{verdict};
  repository_.set_review(profile_id_, *item_id, next, now);
  notice_ = next.has_value()
                ? (*next == GameReviewVerdict::Positive ? "THUMBS UP" : "THUMBS DOWN")
                : "UNREVIEWED";
  refresh();
}

void GameDashboardPresentation::handle_detail_action(Action action,
                                                     std::int64_t now) {
  if (!parent_mode_) return;
  if (action == Action::Back) {
    stage_ = GameDashboardStage::Dashboard;
    detail_item_id_.reset();
    rebuild();
    return;
  }
  if (action == Action::ZoomOut || action == Action::ZoomIn) {
    const int delta = action == Action::ZoomOut ? -1 : 1;
    const auto current = static_cast<long long>(detail_page_);
    detail_page_ = static_cast<GameDetailPage>((current + delta + 3) % 3);
    detail_focus_ = 0;
    return;
  }
  if (action == Action::Up || action == Action::Down) {
    if (detail_page_ == GameDetailPage::Overview) {
      const auto* game = selected_game();
      const auto count = game == nullptr
                             ? 0U
                             : 1U + game->inventory.screenshot_paths.size();
      if (count > 1) {
        const int delta = action == Action::Up ? -1 : 1;
        detail_artwork_focus_ = static_cast<std::size_t>(
            (static_cast<long long>(detail_artwork_focus_) + delta +
             static_cast<long long>(count)) % static_cast<long long>(count));
      }
      return;
    }
    std::size_t count = 1;
    if (detail_page_ == GameDetailPage::Review) count = 3;
    if (detail_page_ == GameDetailPage::Family) count = 3 + child_profiles_.size();
    const int delta = action == Action::Up ? -1 : 1;
    detail_focus_ = static_cast<std::size_t>(
        (static_cast<long long>(detail_focus_) + delta +
         static_cast<long long>(count)) % static_cast<long long>(count));
    return;
  }
  if (action != Action::Confirm || !detail_item_id_.has_value()) return;
  const auto current = repository_.find_for_profile(*detail_item_id_, profile_id_);
  if (!current.has_value()) return;
  if (detail_page_ == GameDetailPage::Review) {
    if (detail_focus_ == 0) toggle_review(GameReviewVerdict::Positive, now);
    else if (detail_focus_ == 1) toggle_review(GameReviewVerdict::Negative, now);
    else repository_.set_completed(profile_id_, *detail_item_id_,
                                    !current->profile.completed, now);
    refresh();
    return;
  }
  if (detail_page_ == GameDetailPage::Family) {
    auto household = current->household;
    household.updated_by = profile_id_;
    household.updated_at = now;
    if (detail_focus_ == 0) {
      household.recommended = !household.recommended;
      if (household.recommended) household.hidden = false;
    }
    else if (detail_focus_ == 1) household.for_kids = !household.for_kids;
    else if (detail_focus_ == 2) {
      household.hidden = !household.hidden;
      if (household.hidden) household.recommended = false;
    } else if (current->inventory.child_eligible) {
      const auto& child = child_profiles_[detail_focus_ - 3];
      const bool assigned = std::find(current->child_allowed_profile_ids.begin(),
                                      current->child_allowed_profile_ids.end(),
                                      child.id) != current->child_allowed_profile_ids.end();
      repository_.set_child_allowed(child.id, *detail_item_id_, !assigned);
    }
    repository_.set_household_state(*detail_item_id_, household);
    refresh();
    return;
  }
}

std::optional<GameDashboardEvent> GameDashboardPresentation::handle(
    Action action, std::int64_t now) {
  notice_.clear();
  // SELECT always leaves the current Sprout session, including a detail or
  // filter view. It must not depend on whichever dashboard sub-view is open.
  if (action == Action::ProfileSelect) {
    return GameDashboardEvent{.type = GameDashboardEventType::BackRequested};
  }
  if (stage_ == GameDashboardStage::Filters) {
    handle_filter_action(action);
    return std::nullopt;
  }
  if (stage_ == GameDashboardStage::Search) {
    if (search_results_focused_ && action == Action::Confirm && !search_results_.empty()) {
      return GameDashboardEvent{.type = GameDashboardEventType::LaunchRequested,
                                .item_id = search_results_[search_result_focus_].inventory.item_id};
    }
    handle_search_action(action, now);
    return std::nullopt;
  }
  if (stage_ == GameDashboardStage::Details) {
    if (action == Action::Confirm && detail_page_ == GameDetailPage::Overview &&
        detail_item_id_.has_value()) {
      return GameDashboardEvent{.type = GameDashboardEventType::LaunchRequested,
                                .item_id = *detail_item_id_};
    }
    handle_detail_action(action, now);
    return std::nullopt;
  }
  if (action == Action::Back) { open_search(); return std::nullopt; }
  if (action == Action::Filters) {
    open_filters();
    return std::nullopt;
  }
  if (action == Action::ClearFilters) {
    const auto settings = std::find_if(rows_.begin(), rows_.end(), [](const auto& row) {
      return row.kind == DashboardRowKind::Settings;
    });
    if (settings != rows_.end()) row_focus_ = static_cast<std::size_t>(settings - rows_.begin());
    return std::nullopt;
  }
  if (action == Action::Up || action == Action::Down) {
    move_row(action == Action::Up ? -1 : 1);
    return std::nullopt;
  }
  if (action == Action::Left || action == Action::Right) {
    move_item(action == Action::Left ? -1 : 1);
    return std::nullopt;
  }
  if (action == Action::ZoomOut || action == Action::ZoomIn) {
    if (!parent_mode_) return std::nullopt;
    toggle_review(action == Action::ZoomIn ? GameReviewVerdict::Positive
                                           : GameReviewVerdict::Negative,
                  now);
    return std::nullopt;
  }
  if ((action != Action::Confirm && action != Action::Menu) || rows_.empty()) return std::nullopt;
  const auto& row = rows_[row_focus_];
  const auto focus = item_focus(row.kind);
  if (!row.games.empty()) {
    const auto& item_id = row.games[std::min(focus, row.games.size() - 1)].item_id;
    if (!parent_mode_ && action == Action::Confirm) {
      return GameDashboardEvent{.type = GameDashboardEventType::LaunchRequested,
                                .item_id = item_id};
    }
    detail_item_id_ = item_id;
    detail_page_ = GameDetailPage::Overview;
    detail_focus_ = 0;
    detail_artwork_focus_ = 0;
    stage_ = GameDashboardStage::Details;
  } else if (!row.settings.empty() && action == Action::Confirm) {
    return GameDashboardEvent{.type = GameDashboardEventType::SettingsRequested,
                              .item_id = row.settings[std::min(focus, row.settings.size() - 1)]};
  } else if (!row.platforms.empty()) {
    model_.toggle_platform(row.platforms[std::min(focus, row.platforms.size() - 1)].platform);
    rebuild();
    const auto all = std::find_if(rows_.begin(), rows_.end(), [](const auto& candidate) {
      return candidate.kind == DashboardRowKind::AllGames;
    });
    if (all != rows_.end()) row_focus_ = static_cast<std::size_t>(all - rows_.begin());
  }
  return std::nullopt;
}

void GameDashboardPresentation::report_launch_result(std::string notice) {
  notice_ = std::move(notice);
  refresh();
}

void GameDashboardPresentation::refresh() {
  const auto filter = model_.filter();
  records_ = repository_.list_for_profile(profile_id_, false);
  if (!parent_mode_) {
    records_.erase(std::remove_if(records_.begin(), records_.end(),
                                  [this](const auto& game) {
      return !game.inventory.child_eligible || game.household.hidden ||
             !game.profile.child_allowed;
    }), records_.end());
  }
  model_ = GameDashboardModel(records_);
  model_.set_filter(filter);
  rebuild();
}

void GameDashboardPresentation::rebuild() {
  rows_ = model_.rows();
  DashboardRow settings;
  settings.kind = DashboardRowKind::Settings;
  settings.settings = parent_mode_
                          ? std::vector<std::string>{"PROFILE", "BACKGROUND", "THEME", "ACCENT", "TILE STYLE", "MOTION", "BIG MODE", "SET PIN", "BACKUP", "LOCK", "PROFILES"}
                          : std::vector<std::string>{"PROFILE", "BACKGROUND", "THEME", "ACCENT", "BIG MODE", "PROFILES"};
  rows_.push_back(std::move(settings));
  available_platforms_ = model_.available_platform_summaries();
  if (rows_.empty()) row_focus_ = 0;
  else if (row_focus_ >= rows_.size()) row_focus_ = rows_.size() - 1;
  for (const auto& row : rows_) {
    const auto count = !row.games.empty() ? row.games.size()
                     : !row.platforms.empty() ? row.platforms.size()
                     : !row.settings.empty() ? row.settings.size()
                     : row.statistics.has_value() ? 4U : 0U;
    auto& focus = item_focus_[row_offset(row.kind)];
    if (count == 0) focus = 0;
    else if (focus >= count) focus = count - 1;
  }
}

}  // namespace sprout::launcher
