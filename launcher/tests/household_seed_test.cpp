#include "sprout/launcher/household_seed.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

}  // namespace

int main() {
  try {
    const auto seed = sprout::launcher::load_household_seed(
        std::filesystem::path(SPROUT_SOURCE_DIR) / "config" / "household-seed.example.json");
    require(seed.profiles.size() == 4, "household seed should define four profiles");
    require(sprout::launcher::seed_includes_title(seed, "child-one", "GB",
                                                   "Kirby's Dream Land (USA)"),
            "child curation should reconcile ROM region suffixes");
    require(!sprout::launcher::seed_includes_title(seed, "child-one", "SFC",
                                                    "Kirby's Dream Land (USA)"),
            "child curation must not cross platform boundaries");
    require(sprout::launcher::seed_favorites_title(seed, "child-one", "ARCADE",
                                                    "Mouse & Cheese Maze"),
            "Sprout Arcade games should be seedable as child favorites");
    require(sprout::launcher::seed_includes_title(seed, "parent-one", "NES",
                                                   "Any local title"),
            "parents should retain their full local library");
    const auto root = std::filesystem::temp_directory_path() /
        "sprout-household-seed-test";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);
    sprout::launcher::ProfileRepository profiles(root / "profiles.sqlite3");
    sprout::launcher::apply_household_seed(profiles, seed);
    require(profiles.list_profiles(false).size() == 4,
            "household seed should create its missing profiles");
    sprout::launcher::apply_household_seed(profiles, seed);
    require(profiles.list_profiles(false).size() == 4,
            "household seed must be idempotent");
    std::filesystem::remove_all(root, ignored);
  } catch (const std::exception& error) {
    std::cerr << "household seed test failed: " << error.what() << '\n';
    return 1;
  }
  std::cout << "household seed tests passed\n";
  return 0;
}
