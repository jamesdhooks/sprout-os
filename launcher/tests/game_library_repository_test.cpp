#include "sprout/launcher/game_library_repository.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using sprout::launcher::DiscoveredGame;
using sprout::launcher::GameLaunchOutcome;
using sprout::launcher::GameLibraryRepository;
using sprout::launcher::GamePlatform;
using sprout::launcher::GameReviewVerdict;
using sprout::launcher::GameSource;
using sprout::launcher::HouseholdGameState;

void require(bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}

class TemporaryDirectory {
 public:
  TemporaryDirectory() {
    const auto nonce =
        std::chrono::high_resolution_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() /
            ("sprout-game-library-test-" + std::to_string(nonce));
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

std::vector<DiscoveredGame> games() {
  return {
      DiscoveredGame{
          .item_id = "onion:GB:Meadow.gb",
          .title = "Meadow",
          .platform = GamePlatform::GameBoy,
          .source = GameSource::Emulated,
          .artwork_path = "covers/meadow.png",
          .screenshot_paths = {"shots/meadow-1.png", "shots/meadow-2.png"},
          .child_eligible = true,
      },
      DiscoveredGame{
          .item_id = "arcade:sprout.snake",
          .title = "Starlight Snake",
          .platform = GamePlatform::SproutArcade,
          .source = GameSource::Native,
          .artwork_path = "covers/snake.png",
          .screenshot_paths = {},
          .child_eligible = true,
      },
  };
}

void platform_identity_is_stable_and_unambiguous() {
  using sprout::launcher::game_platform_from_library_label;
  using sprout::launcher::game_platform_id;
  require(game_platform_from_library_label("ARCADE", GameSource::Native) ==
              GamePlatform::SproutArcade,
          "native ARCADE must map to Sprout Arcade");
  require(game_platform_from_library_label("ARCADE", GameSource::Emulated) ==
              GamePlatform::OnionArcade,
          "emulated ARCADE must map to Onion Arcade");
  require(game_platform_id(GamePlatform::SproutArcade) == "sprout-arcade",
          "Sprout platform ID must be stable");
}

void inventory_state_and_history_survive_reconciliation() {
  TemporaryDirectory directory;
  GameLibraryRepository repository(directory.path() / "library.sqlite3");
  require(repository.database_schema_version() == 1,
          "new game library must use schema v1");
  repository.reconcile_inventory(games(), 100);

  auto records = repository.list_for_profile("parent-one");
  require(records.size() == 2, "both discovered games must be listed");
  require(records[0].inventory.screenshot_paths.size() == 2,
          "screenshots must retain catalogue order");

  repository.set_household_state(
      "onion:GB:Meadow.gb",
      HouseholdGameState{.recommended = true,
                         .for_kids = true,
                         .hidden = false,
                         .updated_by = "parent-one",
                         .updated_at = 110});
  repository.set_favorite("parent-one", "onion:GB:Meadow.gb", true);
  repository.set_child_allowed("child-one", "onion:GB:Meadow.gb", true);
  repository.set_review("parent-one", "onion:GB:Meadow.gb",
                        GameReviewVerdict::Positive, 120);
  repository.set_completed("parent-one", "onion:GB:Meadow.gb", true, 130);

  const auto parent = repository.find_for_profile(
      "onion:GB:Meadow.gb", "parent-one");
  require(parent.has_value() && parent->household.recommended &&
              parent->household.for_kids && parent->profile.favorite &&
              parent->profile.verdict == GameReviewVerdict::Positive &&
              parent->profile.completed,
          "parent and household state must round trip");
  const auto child = repository.find_for_profile(
      "onion:GB:Meadow.gb", "child-one");
  require(child.has_value() && child->profile.child_allowed &&
              !child->profile.verdict.has_value(),
          "profile state must remain isolated");

  repository.reconcile_inventory({games()[1]}, 200);
  const auto missing = repository.find_for_profile(
      "onion:GB:Meadow.gb", "parent-one");
  require(missing.has_value() && !missing->inventory.available &&
              missing->household.recommended && missing->profile.completed,
          "missing games must retain review and completion history");
  require(repository.list_for_profile("parent-one", false).size() == 1,
          "available-only queries must exclude missing games");
}

void hiding_cannot_leave_a_game_recommended() {
  TemporaryDirectory directory;
  GameLibraryRepository repository(directory.path() / "library.sqlite3");
  repository.reconcile_inventory(games(), 100);
  bool rejected = false;
  try {
    repository.set_household_state(
        "arcade:sprout.snake",
        HouseholdGameState{.recommended = true,
                           .hidden = true,
                           .updated_by = "parent-one",
                           .updated_at = 200});
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  require(rejected, "hidden and recommended must be mutually exclusive");
}

void play_sessions_count_only_real_terminal_launches() {
  TemporaryDirectory directory;
  GameLibraryRepository repository(directory.path() / "library.sqlite3");
  repository.reconcile_inventory(games(), 100);
  repository.begin_play_session("session-one", "parent-one",
                                "arcade:sprout.snake", "native", 1000);
  auto active = repository.find_for_profile(
      "arcade:sprout.snake", "parent-one");
  require(active.has_value() && active->play.launch_count == 0,
          "active handoffs must not count as completed launches");
  repository.checkpoint_play_session("session-one", 4500);
  repository.finish_play_session("session-one", GameLaunchOutcome::Completed,
                                 7000, 1010);
  auto completed = repository.find_for_profile(
      "arcade:sprout.snake", "parent-one");
  require(completed.has_value() && completed->play.launch_count == 1 &&
              completed->play.active_milliseconds == 7000 &&
              completed->play.last_played_at == 1000,
          "finished play session must contribute exact activity");

  repository.begin_play_session("session-two", "parent-one",
                                "arcade:sprout.snake", "onion", 2000);
  repository.checkpoint_play_session("session-two", 3000);
  require(repository.recover_interrupted_sessions(2010) == 1,
          "startup recovery must close active handoffs");
  completed = repository.find_for_profile(
      "arcade:sprout.snake", "parent-one");
  require(completed->play.launch_count == 2 &&
              completed->play.active_milliseconds == 10000,
          "interrupted sessions must retain only checkpointed activity");
}

void metadata_flags_are_idempotent() {
  TemporaryDirectory directory;
  GameLibraryRepository repository(directory.path() / "library.sqlite3");
  require(!repository.metadata_flag("seed-import-v1"),
          "missing metadata flag must be false");
  repository.set_metadata_flag("seed-import-v1", true);
  repository.set_metadata_flag("seed-import-v1", true);
  require(repository.metadata_flag("seed-import-v1"),
          "metadata flags must be idempotent");
}

}  // namespace

int main() {
  try {
    platform_identity_is_stable_and_unambiguous();
    inventory_state_and_history_survive_reconciliation();
    hiding_cannot_leave_a_game_recommended();
    play_sessions_count_only_real_terminal_launches();
    metadata_flags_are_idempotent();
    std::cout << "game library repository tests passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "game library repository test failed: " << error.what()
              << '\n';
    return 1;
  }
}
