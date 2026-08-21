#include "sprout/launcher/game_library_integration.hpp"

#include "sprout/launcher/household_seed.hpp"

#include <algorithm>
#include <map>
#include <string_view>
#include <type_traits>
#include <utility>

namespace sprout::launcher {
namespace {

constexpr std::string_view kSeedImportFlag = "household-seed-library-v3";

GameSource source_of(const LibraryEntry& entry) {
  return std::holds_alternative<NativeLaunchTarget>(entry.launch_target)
             ? GameSource::Native
             : GameSource::Emulated;
}

const LibraryEntry* find_seed_match(const SeededLibraryItem& seeded,
                                    const std::vector<LibraryEntry>& entries) {
  const auto found = std::find_if(
      entries.begin(), entries.end(), [&](const auto& entry) {
        return seeded_library_item_matches(seeded, entry.platform_label,
                                           entry.title);
      });
  return found == entries.end() ? nullptr : &*found;
}

std::string unresolved(const SeededProfile& profile, std::string_view kind,
                       const SeededLibraryItem& item) {
  return profile.profile.id + ":" + std::string(kind) + ":" + item.platform +
         ":" + item.title;
}

std::string assignment_flag(const SeededProfile& profile, std::string_view kind,
                            const SeededLibraryItem& item) {
  return std::string(kSeedImportFlag) + ":" + profile.profile.id + ":" +
         std::string(kind) + ":" + item.platform + ":" + item.title;
}

}  // namespace

void reconcile_library_entries(GameLibraryRepository& repository,
                               const std::vector<LibraryEntry>& entries,
                               std::int64_t observed_at) {
  std::vector<DiscoveredGame> discovered;
  discovered.reserve(entries.size());
  for (const auto& entry : entries) {
    const auto source = source_of(entry);
    discovered.push_back({
        .item_id = entry.id,
        .title = entry.title,
        .platform = game_platform_from_library_label(entry.platform_label, source),
        .source = source,
        .artwork_path = entry.artwork_path,
        .screenshot_paths = {},
        .child_eligible = source == GameSource::Emulated || entry.child_visible,
    });
  }
  repository.reconcile_inventory(discovered, observed_at);
}

SeedLibraryMigrationResult migrate_household_seed_library(
    GameLibraryRepository& repository, const HouseholdSeed& seed,
    const std::vector<LibraryEntry>& entries, std::int64_t imported_at) {
  SeedLibraryMigrationResult result;
  if (repository.metadata_flag(kSeedImportFlag)) return result;

  std::map<std::string, HouseholdGameState> household;
  for (const auto& profile : seed.profiles) {
    for (const auto& favorite : profile.favorite_items) {
      const auto flag = assignment_flag(profile, "favorite", favorite);
      if (repository.metadata_flag(flag)) continue;
      const auto* match = find_seed_match(favorite, entries);
      if (match == nullptr) {
        result.unresolved_items.push_back(unresolved(profile, "favorite", favorite));
        continue;
      }
      repository.set_favorite(profile.profile.id, match->id, true);
      if (profile.profile.role == ProfileRole::Child) {
        repository.set_child_allowed(profile.profile.id, match->id, true);
      }
      repository.set_metadata_flag(flag, true);
      result.imported = true;
    }
    for (const auto& curated : profile.curated_items) {
      const auto flag = assignment_flag(profile, "curated", curated);
      if (repository.metadata_flag(flag)) continue;
      const auto* match = find_seed_match(curated, entries);
      if (match == nullptr) {
        result.unresolved_items.push_back(unresolved(profile, "curated", curated));
        continue;
      }
      auto& state = household[match->id];
      state.recommended = true;
      state.for_kids = true;
      state.hidden = false;
      state.updated_by = profile.profile.id;
      state.updated_at = imported_at;
      if (profile.profile.role == ProfileRole::Child) {
        repository.set_child_allowed(profile.profile.id, match->id, true);
      }
      repository.set_metadata_flag(flag, true);
      result.imported = true;
    }
  }
  for (const auto& [item_id, state] : household) {
    repository.set_household_state(item_id, state);
  }
  if (result.unresolved_items.empty()) {
    repository.set_metadata_flag(kSeedImportFlag, true);
  }
  return result;
}

}  // namespace sprout::launcher
