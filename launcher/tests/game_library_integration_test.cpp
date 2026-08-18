#include "sprout/launcher/game_library_integration.hpp"

#include "sprout/launcher/household_seed.hpp"

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
            ("sprout-library-integration-test-" + std::to_string(nonce));
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

LibraryEntry emulated(std::string id, std::string title,
                      std::string platform) {
  return {
      .id = std::move(id),
      .title = std::move(title),
      .platform_label = std::move(platform),
      .artwork_path = "cover.png",
      .launch_target = EmulatedLaunchTarget{
          .item_id = "launch", .system = OnionSystem::GameBoy,
          .rom_path = "game.gb", .launch_allowed = true},
      .child_visible = false,
  };
}

LibraryEntry native(std::string id, std::string title, bool child_eligible) {
  return {
      .id = std::move(id),
      .title = std::move(title),
      .platform_label = "ARCADE",
      .artwork_path = "cover.png",
      .launch_target = NativeLaunchTarget{
          .item_id = "launch", .package_root = "package",
          .profile_id = "parent", .seed = 1, .launch_allowed = true},
      .child_visible = child_eligible,
  };
}

HouseholdSeed seed() {
  return {.profiles = {
      SeededProfile{
          .profile = {.id = "parent-one", .display_name = "Parent",
                      .role = ProfileRole::Parent,
                      .avatar_ref = "builtin:fox",
                      .save_namespace = "parent-one"},
          .favorite_items = {{.platform = "GB", .title = "Meadow (USA)"}},
      },
      SeededProfile{
          .profile = {.id = "child-one", .display_name = "Child",
                      .role = ProfileRole::Child,
                      .avatar_ref = "builtin:fox",
                      .save_namespace = "child-one",
                      .content_policy_ref = "content:child",
                      .time_policy_ref = "time:child"},
          .curated_items = {{.platform = "GB", .title = "Meadow"},
                            {.platform = "ARCADE", .title = "Family Native"}},
          .favorite_items = {{.platform = "GB", .title = "Bonus"}},
      },
  }};
}

void migration_preserves_seed_intent_and_reports_missing_items() {
  TemporaryDirectory directory;
  GameLibraryRepository repository(directory.path() / "library.sqlite3");
  std::vector<LibraryEntry> entries{
      emulated("onion:GB:Meadow.gb", "Meadow", "GB"),
      native("arcade:family", "Family Native", true),
      native("arcade:parent", "Parent Native", false),
  };
  reconcile_library_entries(repository, entries, 100);
  const auto result = migrate_household_seed_library(repository, seed(), entries, 110);
  require(result.imported && result.unresolved_items.size() == 1,
          "first migration must import and report the missing favorite");
  const auto waiting =
      migrate_household_seed_library(repository, seed(), entries, 120);
  require(!waiting.imported && waiting.unresolved_items.size() == 1,
          "completed assignments must remain idempotent while missing games retry");
  entries.push_back(emulated("onion:GB:Bonus.gb", "Bonus", "GB"));
  reconcile_library_entries(repository, entries, 125);
  const auto completed =
      migrate_household_seed_library(repository, seed(), entries, 130);
  require(completed.imported && completed.unresolved_items.empty() &&
              !migrate_household_seed_library(repository, seed(), entries, 140)
                   .imported,
          "a later ROM install must complete its pending seed assignment once");

  const auto parent = repository.find_for_profile("onion:GB:Meadow.gb", "parent-one");
  require(parent.has_value() && parent->profile.favorite &&
              parent->household.recommended && parent->household.for_kids,
          "seed favorites and household curation must persist separately");
  const auto child = repository.find_for_profile("arcade:family", "child-one");
  require(child.has_value() && child->profile.child_allowed,
          "exact child membership must be persisted");
}

}  // namespace

int main() {
  try {
    migration_preserves_seed_intent_and_reports_missing_items();
    std::cout << "game library integration tests passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "game library integration test failed: " << error.what() << '\n';
    return 1;
  }
}
