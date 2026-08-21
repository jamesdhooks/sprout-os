#pragma once

#include "sprout/launcher/game_library_repository.hpp"
#include "sprout/launcher/game_library_entry.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace sprout::launcher {

struct HouseholdSeed;

struct SeedLibraryMigrationResult {
  bool imported{false};
  std::vector<std::string> unresolved_items;
};

void reconcile_library_entries(GameLibraryRepository& repository,
                               const std::vector<LibraryEntry>& entries,
                               std::int64_t observed_at);

[[nodiscard]] SeedLibraryMigrationResult migrate_household_seed_library(
    GameLibraryRepository& repository, const HouseholdSeed& seed,
    const std::vector<LibraryEntry>& entries, std::int64_t imported_at);


}  // namespace sprout::launcher
