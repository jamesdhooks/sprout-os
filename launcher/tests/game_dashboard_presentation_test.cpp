#include "sprout/launcher/game_dashboard_presentation.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace sprout::launcher;

void require(bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}

class TemporaryDirectory {
 public:
  TemporaryDirectory() {
    const auto nonce =
        std::chrono::high_resolution_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() /
            ("sprout-dashboard-presentation-test-" + std::to_string(nonce));
    std::filesystem::create_directories(path_);
  }
  ~TemporaryDirectory() {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }
  const std::filesystem::path& path() const noexcept { return path_; }

 private:
  std::filesystem::path path_;
};

std::vector<DiscoveredGame> inventory() {
  return {
      {.item_id = "gb-one", .title = "GB One",
       .platform = GamePlatform::GameBoy, .source = GameSource::Emulated,
       .child_eligible = true},
      {.item_id = "snes-one", .title = "SNES One",
       .platform = GamePlatform::SuperNintendo, .source = GameSource::Emulated,
       .child_eligible = true},
      {.item_id = "parent-only", .title = "Parent Only",
       .platform = GamePlatform::SproutArcade, .source = GameSource::Native,
       .child_eligible = false},
  };
}

std::vector<Profile> children() {
  return {{.id = "child-one", .display_name = "Kid",
           .role = ProfileRole::Child, .accent_rgb = 0,
           .avatar_ref = "builtin:fox"}};
}

void quick_review_and_detail_actions_persist() {
  TemporaryDirectory directory;
  GameLibraryRepository repository(directory.path() / "library.sqlite3");
  repository.reconcile_inventory(inventory(), 100);
  repository.set_household_state(
      "gb-one", {.recommended = true, .for_kids = true,
                  .updated_by = "parent-one", .updated_at = 100});
  GameDashboardPresentation dashboard(repository, "parent-one", children());

  require(dashboard.stage() == GameDashboardStage::Dashboard &&
              !dashboard.rows().empty(),
          "dashboard must begin on the flat carousel surface");
  (void)dashboard.handle(Action::ZoomIn, 200);
  const auto reviewed = repository.find_for_profile("gb-one", "parent-one");
  require(reviewed.has_value() &&
              reviewed->profile.verdict == GameReviewVerdict::Positive,
          "right shoulder must apply a positive review to the focused game");
  (void)dashboard.handle(Action::ZoomIn, 210);
  require(!repository.find_for_profile("gb-one", "parent-one")
               ->profile.verdict.has_value(),
          "repeating a verdict must clear it to unreviewed");

  (void)dashboard.handle(Action::Confirm, 220);
  require(dashboard.stage() == GameDashboardStage::Details &&
              dashboard.selected_game() != nullptr,
          "A on a game card must open dedicated details");
  (void)dashboard.handle(Action::ZoomIn, 220);
  require(dashboard.detail_page() == GameDetailPage::Review,
          "shoulders must switch detail pages");
  (void)dashboard.handle(Action::Down, 220);
  (void)dashboard.handle(Action::Down, 220);
  (void)dashboard.handle(Action::Confirm, 230);
  require(repository.find_for_profile("gb-one", "parent-one")->profile.completed,
          "completion must persist independently from review");
}

void filters_apply_live_with_vertical_option_navigation() {
  TemporaryDirectory directory;
  GameLibraryRepository repository(directory.path() / "library.sqlite3");
  repository.reconcile_inventory(inventory(), 100);
  GameDashboardPresentation dashboard(repository, "parent-one", children());
  (void)dashboard.handle(Action::Filters, 200);
  require(dashboard.stage() == GameDashboardStage::Filters,
          "X must open the filter drawer");
  (void)dashboard.handle(Action::Down, 200);
  (void)dashboard.handle(Action::Confirm, 200);
  require(dashboard.filter().platforms.size() == 1,
          "A must apply the focused filter immediately");
  (void)dashboard.handle(Action::Back, 200);
  (void)dashboard.handle(Action::ClearFilters, 200);
  require(dashboard.rows()[dashboard.row_focus()].kind == DashboardRowKind::Settings,
          "Y must jump directly to the settings row");
}

void carousel_navigation_stops_at_its_edges() {
  TemporaryDirectory directory;
  GameLibraryRepository repository(directory.path() / "library.sqlite3");
  repository.reconcile_inventory(inventory(), 100);
  GameDashboardPresentation dashboard(repository, "parent-one", children());

  (void)dashboard.handle(Action::Left, 200);
  (void)dashboard.handle(Action::Up, 200);
  require(dashboard.row_focus() == 0 &&
              dashboard.item_focus(dashboard.rows()[0].kind) == 0,
          "carousel must not wrap before its first row or card");

  for (std::size_t index = 0; index < dashboard.rows().size() + 4; ++index) {
    (void)dashboard.handle(Action::Down, 200);
  }
  require(dashboard.row_focus() == dashboard.rows().size() - 1,
          "carousel must stop on its final row");
  const auto final_kind = dashboard.rows()[dashboard.row_focus()].kind;
  for (int index = 0; index < 20; ++index) {
    (void)dashboard.handle(Action::Right, 200);
  }
  const auto final_focus = dashboard.item_focus(final_kind);
  (void)dashboard.handle(Action::Right, 200);
  require(dashboard.item_focus(final_kind) == final_focus,
          "carousel must stop on its final card");

  const auto menu_event = dashboard.handle(Action::Menu, 200);
  require(!menu_event.has_value(),
          "GameSwitcher must not reveal a secondary Sprout menu");

  (void)dashboard.handle(Action::Filters, 200);
  require(dashboard.stage() == GameDashboardStage::Filters,
          "test must enter filters before checking Back");
  (void)dashboard.handle(Action::Back, 200);
  require(dashboard.stage() == GameDashboardStage::Dashboard,
          "Back must return from filters to the primary dashboard");

  for (int step = 0; step < 12 &&
       dashboard.rows()[dashboard.row_focus()].games.empty(); ++step) {
    (void)dashboard.handle(Action::Up, 200);
  }
  require(!dashboard.rows()[dashboard.row_focus()].games.empty(),
          "test must find a game row before checking Start");
  (void)dashboard.handle(Action::Menu, 200);
  require(dashboard.stage() == GameDashboardStage::Details,
          "Start must open the selected game's detail view");
}

void family_assignments_fail_closed_for_parent_only_games() {
  TemporaryDirectory directory;
  GameLibraryRepository repository(directory.path() / "library.sqlite3");
  repository.reconcile_inventory(inventory(), 100);
  GameDashboardPresentation dashboard(repository, "parent-one", children());

  for (int step = 0; step < 12 &&
       (dashboard.rows()[dashboard.row_focus()].games.empty() ||
        dashboard.rows()[dashboard.row_focus()].games[
            dashboard.item_focus(dashboard.rows()[dashboard.row_focus()].kind)].item_id !=
            "parent-only"); ++step) {
    (void)dashboard.handle(Action::Right, 200);
  }
  require(dashboard.rows()[dashboard.row_focus()].games[
              dashboard.item_focus(dashboard.rows()[dashboard.row_focus()].kind)].item_id ==
              "parent-only",
          "test fixture must locate the parent-only card");
  (void)dashboard.handle(Action::Confirm, 200);
  (void)dashboard.handle(Action::ZoomOut, 200);
  require(dashboard.detail_page() == GameDetailPage::Family,
          "left shoulder from Overview must open Family");
  for (int step = 0; step < 3; ++step) (void)dashboard.handle(Action::Down, 200);
  (void)dashboard.handle(Action::Confirm, 200);
  require(repository.find_for_profile("parent-only", "child-one")
              ->child_allowed_profile_ids.empty(),
          "parent-only packages must reject child assignment");
}

void recommending_a_hidden_game_restores_it_atomically() {
  TemporaryDirectory directory;
  GameLibraryRepository repository(directory.path() / "library.sqlite3");
  repository.reconcile_inventory(inventory(), 100);
  GameDashboardPresentation dashboard(repository, "parent-one", children());
  (void)dashboard.handle(Action::Confirm, 200);
  (void)dashboard.handle(Action::ZoomOut, 200);
  (void)dashboard.handle(Action::Down, 200);
  (void)dashboard.handle(Action::Down, 200);
  (void)dashboard.handle(Action::Confirm, 200);
  require(repository.find_for_profile("gb-one", "parent-one")->household.hidden,
          "Hidden toggle must persist");
  (void)dashboard.handle(Action::Down, 200);
  (void)dashboard.handle(Action::Down, 200);
  (void)dashboard.handle(Action::Confirm, 200);
  const auto restored = repository.find_for_profile("gb-one", "parent-one");
  require(restored->household.recommended && !restored->household.hidden,
          "recommending a hidden game must restore it without an invalid state");
}

}  // namespace

int main() {
  try {
    quick_review_and_detail_actions_persist();
    filters_apply_live_with_vertical_option_navigation();
    carousel_navigation_stops_at_its_edges();
    family_assignments_fail_closed_for_parent_only_games();
    recommending_a_hidden_game_restores_it_atomically();
    std::cout << "game dashboard presentation tests passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "game dashboard presentation test failed: " << error.what()
              << '\n';
    return 1;
  }
}
