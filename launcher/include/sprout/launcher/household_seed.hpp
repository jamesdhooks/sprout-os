#pragma once

#include "sprout/launcher/profile_repository.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace sprout::launcher {

struct SeededLibraryItem {
  std::string platform;
  std::string title;
};

struct SeededProfile {
  NewProfile profile;
  std::vector<SeededLibraryItem> curated_items;
  std::vector<SeededLibraryItem> favorite_items;
};

struct HouseholdSeed {
  std::vector<SeededProfile> profiles;
};

[[nodiscard]] HouseholdSeed load_household_seed(
    const std::filesystem::path& path);
void apply_household_seed(ProfileRepository& repository,
                          const HouseholdSeed& seed);
[[nodiscard]] bool seed_includes_title(const HouseholdSeed& seed,
                                        const std::string& profile_id,
                                        const std::string& platform,
                                        const std::string& title);
[[nodiscard]] bool seed_favorites_title(const HouseholdSeed& seed,
                                         const std::string& profile_id,
                                         const std::string& platform,
                                         const std::string& title);

}  // namespace sprout::launcher
